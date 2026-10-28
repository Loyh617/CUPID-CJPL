/// \file include/RunAction.hh
/// \brief Declaration: Manage selected output for a run and coordinate completed worker files.
///
/// ActionInitialization constructs this action; Geant4 invokes the run hooks.
/// BeginOfRunAction() opens the worker or serial file. EventAction writes via
/// GetOutput(). EndOfRunAction() closes files; the MT master merges and
/// removes worker files only after validation. writeOutput=false is for the
/// MT coordinator and does not disable its merge or summary responsibilities.

#pragma once

#include "output/OutputManager.hh"

#include "G4UserRunAction.hh"
#include "globals.hh"

namespace cupid {
class RunAction final : public G4UserRunAction {
 public:
  // Select the registered backend here; use the same name for master and workers.
  explicit RunAction(const G4String& outputDirectory, G4bool writeOutput = true,
                     const G4String& outputName = "RootOutput");
  void BeginOfRunAction(const G4Run*) override;
  void EndOfRunAction(const G4Run*) override;
  OutputManager& GetOutput() { return fOutput; }
  G4int GetRunId() const { return fRunId; }

 private:
  OutputManager fOutput;
  const G4String fOutputDirectory;
  const G4bool fWriteOutput;
  G4String fFileName;
  G4int fRunId = -1;
};
}  // namespace cupid
