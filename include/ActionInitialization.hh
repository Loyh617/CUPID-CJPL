/// \file include/ActionInitialization.hh
/// \brief Declaration: Connect the Geant4 user actions and their per-thread dependencies.
///
/// Register with the run manager in main(). Geant4 calls Build() for event
/// processing and BuildForMaster() for the MT coordinator. Each worker owns
/// its actions; EventAction shares that worker's RunAction and track ancestry.
/// The geometry manager supplies the scoring-volume names at run start.

#pragma once

#include "G4VUserActionInitialization.hh"
#include "globals.hh"

#include <atomic>
#include <memory>

namespace cupid {
class GeneratorMessenger;
class GeometryManager;
class RunAction;
class ActionInitialization final : public G4VUserActionInitialization {
 public:
  ActionInitialization(const G4String& outputDirectory, const GeometryManager& geometry);
  ~ActionInitialization() override;
  void BuildForMaster() const override;
  void Build() const override;
  bool HasGeneratorError() const { return fGeneratorFailed.load(); }

 private:
  RunAction* CreateRunAction() const;
  const GeometryManager& fGeometry;
  const G4String fOutputDirectory;
  // Set on the master in PreInit; read-only while events are processed.
  // Override with /cupid/generator/select and verbose before /run/initialize.
  G4String fGeneratorSelection = "gun";
  G4int fGeneratorVerbosity = 0;
  std::unique_ptr<GeneratorMessenger> fGeneratorMessenger;
  mutable std::atomic_bool fGeneratorFailed{false};
};
}
