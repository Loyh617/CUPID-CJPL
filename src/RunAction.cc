/// \file src/RunAction.cc
/// \brief Drive native analysis booking, file naming, finalization and result logs.
///
/// All threads book the same selected types and detector names for each run.
/// Geant4 merges ntuples during Write(); no external worker files are merged.
/// Each recorder has a separate file; empty files are removed and reported in run.log.

#include "RunAction.hh"
#include "record/RecordManager.hh"

#include "G4GenericMessenger.hh"
#include "G4MTRunManager.hh"
#include "G4Run.hh"
#include "G4RunManager.hh"
#include "G4Threading.hh"
#include "G4UImanager.hh"
#include "G4ios.hh"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <string>

namespace cupid {
RunAction::RunAction(const G4String& outputDirectory, const std::vector<G4String>& recorders,
                     const G4String& outputName, DetectorNames detectorNames)
    : fOutput(outputName), fOutputDirectory(outputDirectory), fRecorderNames(recorders),
      fDetectorNames(std::move(detectorNames)) {
  const RecordManager selection(recorders);
  if (selection.IsEnabled("EventEnergyRecorder")) fOutput.BookEventEnergy();
  if (selection.IsEnabled("IncidentEnergyRecorder")) fOutput.BookIncidentEnergy();
  if (!G4Threading::IsWorkerThread()) {
    // A master-only command avoids worker-local timestamps or competing UI commands.
    fMessenger = std::make_unique<G4GenericMessenger>(this, "/cupid/output/", "Run output naming.");
    fMessenger->DeclareProperty("fileName", fFileName,
        "Set a filename prefix (optional .root) or auto for local date/time. Persists until changed.")
        .SetStates(G4State_Idle).SetToBeBroadcasted(false)
        .SetParameterName("prefix", false);
  }
}

RunAction::~RunAction() = default;

G4String RunAction::ResolveFileName() {
  std::string prefix = fFileName;
  if (fFileName == "auto") {
    // Sample local time once on the master; all workers use the resolved name.
    const auto seconds = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const auto* local = std::localtime(&seconds);
    if (!local) throw std::runtime_error("Cannot format the run start time.");
    std::ostringstream name;
    name << "run" << std::put_time(local, "%Y%m%d_%H%M%S");
    prefix = name.str();
  } else {
    // --output controls the directory. Restrict this setting to a portable basename.
    if (prefix.size() >= 5 && prefix.compare(prefix.size() - 5, 5, ".root") == 0)
      prefix.resize(prefix.size() - 5);
    static const std::regex valid("[A-Za-z0-9][A-Za-z0-9_.-]*");
    if (!std::regex_match(prefix, valid) || prefix.back() == '.')
      throw std::invalid_argument("Output prefix must start with a letter/digit and contain only letters, digits, _, - or .; no paths or spaces.");
  }
  // Check the whole enabled recorder group before choosing a shared suffix.
  // Names used by empty runs remain reserved for this RunAction's lifetime.
  const auto directory = std::filesystem::path(std::string(fOutputDirectory));
  for (std::size_t suffix = 0; ; ++suffix) {
    const G4String name = prefix + (suffix == 0 ? "" : "_" + std::to_string(suffix))
                         + fOutput.GetFileExtension();
    if (fUsedFileNames.count(name)) continue;
    const auto baseName = (directory / std::string(name)).string();
    auto files = fOutput.GetFileNames(baseName);
    files.push_back(baseName);
    bool occupied = false;
    for (const auto& file : files) {
      if (std::filesystem::exists(file)) {
        occupied = true;
        break;
      }
    }
    if (!occupied) {
      fUsedFileNames.insert(name);
      return name;
    }
  }
}

void RunAction::BeginOfRunAction(const G4Run* run) {
  fRunId = run->GetRunID();
  const auto threadId = G4Threading::G4GetThreadId();
  const auto directory = std::filesystem::path(std::string(fOutputDirectory));
  fOutput.SetDetectorNames(fDetectorNames ? fDetectorNames() : std::vector<G4String>{});
  if (IsMaster()) {
    fRunFileName = ResolveFileName();
    const auto finalName = directory / std::string(fRunFileName);
    if (std::filesystem::exists(finalName))
      throw std::runtime_error("Run output already exists: " + finalName.string());
    G4cout << "CUPID_OUTPUT_NAME run=" << fRunId << " requested=" << fFileName
           << " base=" << fRunFileName << " clock=local" << G4endl;
    G4cout << "CUPID_OUTPUT_MODULE name=" << fOutput.GetName()
           << " backend=G4AnalysisManager schema=4" << G4endl;
    G4cout << "CUPID_RUN_BEGIN run=" << fRunId << " requested_events=" << run->GetNumberOfEventToBeProcessed()
           << " threads=" << G4RunManager::GetRunManager()->GetNumberOfThreads() << G4endl;
    G4cout << "CUPID_RANDOM_STATE_BEGIN\n" << run->GetRandomNumberStatus() << "\nCUPID_RANDOM_STATE_END" << G4endl;
  } else {
    // G4RunManager calls the master's BeginOfRunAction before starting workers.
    // Read its finalized name; do not sample the clock independently on each worker.
    const auto* masterManager = G4MTRunManager::GetMasterRunManager();
    const auto* master = masterManager
        ? dynamic_cast<const RunAction*>(masterManager->GetUserRunAction()) : nullptr;
    if (!master || master->fRunId != fRunId || master->fRunFileName.empty())
      throw std::runtime_error("Worker output name is unavailable from the master RunAction.");
    fRunFileName = master->fRunFileName;
  }
  if (threadId <= 0) {
    auto* ui = G4UImanager::GetUIpointer();
    for (const char* command : {"/gun/particle", "/gun/ion", "/gun/energy", "/gun/position", "/gun/direction", "/gun/number"})
      G4cout << "CUPID_SOURCE " << command << " " << ui->GetCurrentValues(command) << G4endl;
  }
  const auto baseName = (directory / std::string(fRunFileName)).string();
  fFileNames = fOutput.GetFileNames(baseName);
  fOutput.OpenFile(baseName);
}

void RunAction::EndOfRunAction(const G4Run* run) {
  // Every thread participates in native Write/CloseFile, including the MT master.
  fOutput.CloseFile();
  if (!IsMaster()) return;
  // Geant4 has completed worker run-end callbacks before this master callback.
  OutputSummary summary;
  const RecordManager selection(fRecorderNames);
  if (selection.IsEnabled("EventEnergyRecorder")) summary.rows["EventEnergy"] = 0;
  if (selection.IsEnabled("IncidentEnergyRecorder")) summary.rows["IncidentEnergy"] = 0;
  std::string files;
  for (const auto& fileName : fFileNames) {
    if (!std::filesystem::exists(fileName)) continue;
    const auto counts = fOutput.InspectFile(fileName);
    for (const auto& [name, count] : counts.rows) summary.rows[name] += count;
    if (!files.empty()) files += ",";
    files += fileName;
    G4cout << "CUPID_OUTPUT_FILE file=" << fileName;
    for (const auto& [name, count] : counts.rows) G4cout << " rows_" << name << "=" << count;
    G4cout << G4endl;
  }
  G4cout << "CUPID_RUN_RESULT run=" << fRunId
         << " requested_events=" << run->GetNumberOfEventToBeProcessed()
         << " processed_events=" << run->GetNumberOfEvent();
  for (const auto& [name, count] : summary.rows) G4cout << " rows_" << name << "=" << count;
  G4cout << " file=" << (files.empty() ? "none(empty)" : files) << G4endl;
}
}  // namespace cupid
