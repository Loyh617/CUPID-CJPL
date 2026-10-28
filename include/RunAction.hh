/// \file include/RunAction.hh
/// \brief Configure matching output schemas and manage native analysis run hooks.
///
/// ActionInitialization passes identical recorder names to master and workers.
/// All threads open and finalize output; Geant4 handles merging. Only event
/// threads fill records. The master inspects the completed file and logs counts.

#pragma once
#include "output/OutputManager.hh"

#include "G4UserRunAction.hh"
#include "globals.hh"

#include <vector>

namespace cupid {
class RunAction final : public G4UserRunAction {
 public:
  RunAction(const G4String& outputDirectory, const std::vector<G4String>& recorders,
            const G4String& outputName = "RootOutput");
  void BeginOfRunAction(const G4Run*) override;
  void EndOfRunAction(const G4Run*) override;
  OutputManager& GetOutput() { return fOutput; }
  G4int GetRunId() const { return fRunId; }
  const std::vector<G4String>& GetRecorderNames() const { return fRecorderNames; }

 private:
  OutputManager fOutput;
  const G4String fOutputDirectory;
  const std::vector<G4String> fRecorderNames;
  G4String fFileName;
  G4int fRunId = -1;
};
}  // namespace cupid
