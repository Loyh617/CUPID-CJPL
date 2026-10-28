/// \file src/RunAction.cc
/// \brief Implementation: Manage selected output for a run and coordinate completed worker files.
///
/// ActionInitialization constructs this action; Geant4 invokes the run hooks.
/// BeginOfRunAction() opens the worker or serial file. EventAction writes via
/// GetOutput(). EndOfRunAction() closes files; the MT master merges and
/// removes worker files only after validation. writeOutput=false is for the
/// MT coordinator and does not disable its merge or summary responsibilities.

#include "RunAction.hh"

#include "G4Run.hh"
#include "G4RunManager.hh"
#include "G4Threading.hh"
#include "G4UImanager.hh"
#include "G4ios.hh"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace cupid {
RunAction::RunAction(const G4String& outputDirectory, G4bool writeOutput, const G4String& outputName)
    : fOutput(outputName), fOutputDirectory(outputDirectory), fWriteOutput(writeOutput) {}

void RunAction::BeginOfRunAction(const G4Run* run) {
  fRunId = run->GetRunID();
  const auto threadId = G4Threading::G4GetThreadId();
  const auto directory = std::filesystem::path(std::string(fOutputDirectory));
  if (IsMaster()) {
    const auto finalName = directory / ("run" + std::to_string(fRunId) + fOutput.GetFileExtension());
    if (std::filesystem::exists(finalName) || std::filesystem::exists(finalName.string() + ".partial"))
      throw std::runtime_error("Run output already exists: " + finalName.string());
    G4cout << "CUPID_OUTPUT_MODULE name=" << fOutput.GetName() << G4endl;
    G4cout << "CUPID_RUN_BEGIN run=" << fRunId << " requested_events=" << run->GetNumberOfEventToBeProcessed()
           << " threads=" << G4RunManager::GetRunManager()->GetNumberOfThreads() << G4endl;
    G4cout << "CUPID_RANDOM_STATE_BEGIN\n" << run->GetRandomNumberStatus() << "\nCUPID_RANDOM_STATE_END" << G4endl;
  }
  if (!fWriteOutput) return;
  if (threadId <= 0) {
    auto* ui = G4UImanager::GetUIpointer();
    for (const char* command : {"/gun/particle", "/gun/ion", "/gun/energy", "/gun/position", "/gun/direction", "/gun/number"})
      G4cout << "CUPID_SOURCE " << command << " " << ui->GetCurrentValues(command) << G4endl;
  }
  const auto suffix = threadId >= 0 ? "_worker" + std::to_string(threadId) : "";
  fFileName = (directory / ("run" + std::to_string(fRunId) + suffix + fOutput.GetFileExtension())).string();
  fOutput.OpenFile(fFileName);
}

void RunAction::EndOfRunAction(const G4Run* run) {
  if (fWriteOutput) {
    fOutput.CloseFile();
    G4cout << "CUPID_OUTPUT run=" << fRunId << " thread=" << G4Threading::G4GetThreadId()
           << " events=" << run->GetNumberOfEvent() << " file=" << fFileName << G4endl;
  }
  if (!IsMaster()) return;
  const auto directory = std::filesystem::path(std::string(fOutputDirectory));
  const auto finalName = (directory / ("run" + std::to_string(fRunId) + fOutput.GetFileExtension())).string();
  OutputSummary summary;
  if (!fWriteOutput) {
    // Geant4's MT run-end barrier has completed all worker EndOfRunAction calls.
    std::vector<std::string> inputs;
    const auto workers = G4RunManager::GetRunManager()->GetNumberOfThreads();
    for (G4int id = 0; id < workers; ++id)
      inputs.push_back((directory / ("run" + std::to_string(fRunId) + "_worker" + std::to_string(id) + fOutput.GetFileExtension())).string());
    summary = fOutput.MergeFiles(finalName, inputs);
    // Remove only this run's explicit input list, after the merged file has passed validation.
    for (const auto& input : inputs)
      if (!std::filesystem::remove(input)) throw std::runtime_error("Cannot remove merged input: " + input);
  } else {
    summary = fOutput.InspectFile(finalName);
  }
  G4cout << "CUPID_RUN_RESULT run=" << fRunId << " requested_events=" << run->GetNumberOfEventToBeProcessed()
         << " processed_events=" << run->GetNumberOfEvent()
         << " event_rows=" << summary.eventRows << " incident_rows=" << summary.incidentRows
         << " energy_unit=keV file=" << finalName << G4endl;
}
}  // namespace cupid
