#pragma once

#include "output/RootOutput.hh"

#include "G4UserRunAction.hh"
#include "globals.hh"

namespace cupid {
class RunAction final : public G4UserRunAction {
 public:
  explicit RunAction(const G4String& outputDirectory, G4bool writeOutput = true);
  void BeginOfRunAction(const G4Run*) override;
  void EndOfRunAction(const G4Run*) override;
  RootOutput& GetOutput() { return fOutput; }
  G4int GetRunId() const { return fRunId; }

 private:
  RootOutput fOutput;
  const G4String fOutputDirectory;
  const G4bool fWriteOutput;
  G4String fFileName;
  G4int fRunId = -1;
};
}  // namespace cupid
