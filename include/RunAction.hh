/// \file include/RunAction.hh
/// \brief Configure matching output schemas and manage native analysis run hooks.
///
/// ActionInitialization passes identical recorder names to master and workers.
/// All threads open and finalize output; Geant4 handles merging. Only event
/// threads fill records. A geometry callback supplies names at run start, after
/// geometry construction. The master inspects completed recorder files and logs counts.

#pragma once
#include "output/OutputManager.hh"

#include "G4UserRunAction.hh"
#include "globals.hh"

#include <functional>
#include <memory>
#include <vector>

class G4GenericMessenger;

namespace cupid {
class RunLog;
class RunAction final : public G4UserRunAction {
 public:
  using DetectorNames = std::function<std::vector<G4String>()>;
  RunAction(const G4String& outputDirectory, const std::vector<G4String>& recorders,
            const G4String& outputName = "RootOutput", DetectorNames detectorNames = {},
            RunLog* log = nullptr);
  ~RunAction() override;
  // Set /cupid/output/directory and fileName in Idle, before the next /run/beamOn.
  void BeginOfRunAction(const G4Run*) override;
  void EndOfRunAction(const G4Run*) override;
  OutputManager& GetOutput() { return fOutput; }
  G4int GetRunId() const { return fRunId; }
  const std::vector<G4String>& GetRecorderNames() const { return fRecorderNames; }

 private:
  void PrepareRunDirectory();
  // Only the master/serial action owns the UI command and chooses the run name.
  // "auto" generates local date/time; an explicit prefix persists until changed.
  G4String fFileName = "auto";
  G4String fRunFileName;
  G4String fRunDirectory;  // Absolute, finalized by the master before workers start.
  RunLog* fLog;  // Non-owning; main keeps the shared console capture alive through shutdown.
  std::unique_ptr<G4GenericMessenger> fMessenger;
  OutputManager fOutput;
  // Parent directory, relative to the process working directory; editable between runs.
  G4String fOutputDirectory;
  const std::vector<G4String> fRecorderNames;
  DetectorNames fDetectorNames;
  std::vector<std::string> fFileNames;
  G4int fRunId = -1;
};
}  // namespace cupid
