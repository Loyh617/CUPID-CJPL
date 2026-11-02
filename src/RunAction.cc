/// \file src/RunAction.cc
/// \brief Drive native analysis booking, file naming, finalization and result logs.
///
/// All threads book the same selected types and detector names for each run.
/// Geant4 merges ntuples during Write(); no external worker files are merged.
/// Each recorder has a separate file; empty files are removed and reported in run.log.

#include "RunAction.hh"
#include "record/RecordManager.hh"

#include "G4Run.hh"
#include "G4RunManager.hh"
#include "G4Threading.hh"
#include "G4UImanager.hh"
#include "G4ios.hh"

#include <filesystem>
#include <stdexcept>
#include <utility>
#include <string>
#include <variant>

namespace cupid {
RunAction::RunAction(const G4String& outputDirectory, const std::vector<G4String>& recorders,
                     const G4String& outputName, DetectorNames detectorNames)
    : fOutput(outputName), fOutputDirectory(outputDirectory), fRecorderNames(recorders),
      fDetectorNames(std::move(detectorNames)) {
  const RecordManager selection(recorders);
  for (const auto& batch : selection.GetBatches())
    std::visit([&](const auto& rows) { fOutput.Book(rows); }, batch);
}

void RunAction::BeginOfRunAction(const G4Run* run) {
  fRunId = run->GetRunID();
  const auto threadId = G4Threading::G4GetThreadId();
  const auto directory = std::filesystem::path(std::string(fOutputDirectory));
  fOutput.SetDetectorNames(fDetectorNames ? fDetectorNames() : std::vector<G4String>{});
  if (IsMaster()) {
    const auto finalName = directory / ("run" + std::to_string(fRunId) + fOutput.GetFileExtension());
    if (std::filesystem::exists(finalName))
      throw std::runtime_error("Run output already exists: " + finalName.string());
    G4cout << "CUPID_OUTPUT_MODULE name=" << fOutput.GetName()
           << " backend=G4AnalysisManager schema=4 energy_unit=keV" << G4endl;
    G4cout << "CUPID_RUN_BEGIN run=" << fRunId << " requested_events=" << run->GetNumberOfEventToBeProcessed()
           << " threads=" << G4RunManager::GetRunManager()->GetNumberOfThreads() << G4endl;
    G4cout << "CUPID_RANDOM_STATE_BEGIN\n" << run->GetRandomNumberStatus() << "\nCUPID_RANDOM_STATE_END" << G4endl;
  }
  if (threadId <= 0) {
    auto* ui = G4UImanager::GetUIpointer();
    for (const char* command : {"/gun/particle", "/gun/ion", "/gun/energy", "/gun/position", "/gun/direction", "/gun/number"})
      G4cout << "CUPID_SOURCE " << command << " " << ui->GetCurrentValues(command) << G4endl;
  }
  const auto baseName = (directory / ("run" + std::to_string(fRunId) + fOutput.GetFileExtension())).string();
  fFileNames = fOutput.GetFileNames(baseName);
  fOutput.OpenFile(baseName);
}

void RunAction::EndOfRunAction(const G4Run* run) {
  // Every thread participates in native Write/CloseFile, including the MT master.
  fOutput.CloseFile();
  if (!IsMaster()) return;
  // Geant4 has completed worker run-end callbacks before this master callback.
  OutputSummary summary;
  std::string files;
  for (const auto& fileName : fFileNames) {
    if (!std::filesystem::exists(fileName)) continue;
    const auto counts = fOutput.InspectFile(fileName);
    summary.eventRows += counts.eventRows;
    summary.incidentRows += counts.incidentRows;
    if (!files.empty()) files += ",";
    files += fileName;
    G4cout << "CUPID_OUTPUT_FILE file=" << fileName << " event_rows=" << counts.eventRows
           << " incident_rows=" << counts.incidentRows << G4endl;
  }
  G4cout << "CUPID_RUN_RESULT run=" << fRunId
         << " requested_events=" << run->GetNumberOfEventToBeProcessed()
         << " processed_events=" << run->GetNumberOfEvent()
         << " event_rows=" << summary.eventRows << " incident_rows=" << summary.incidentRows
         << " energy_unit=keV file=" << (files.empty() ? "none(empty)" : files) << G4endl;
}
}  // namespace cupid
