/// \file include/ActionInitialization.hh
/// \brief Declaration: Connect the Geant4 user actions and their per-thread dependencies.
///
/// Register with the run manager in main(). Geant4 calls Build() for event
/// processing and BuildForMaster() for the MT coordinator. Each worker owns
/// its actions; EventAction shares that worker's RunAction and track ancestry.

#pragma once

#include "G4VUserActionInitialization.hh"
#include "globals.hh"

#include <atomic>
#include <memory>

namespace cupid {
class GeneratorMessenger;
class ActionInitialization final : public G4VUserActionInitialization {
 public:
  explicit ActionInitialization(const G4String& outputDirectory);
  ~ActionInitialization() override;
  void BuildForMaster() const override;
  void Build() const override;
  bool HasGeneratorError() const { return fGeneratorFailed.load(); }

 private:
  const G4String fOutputDirectory;
  // Set on the master in PreInit; read-only while events are processed.
  G4String fGeneratorSelection = "gun";
  G4int fGeneratorVerbosity = 0;
  std::unique_ptr<GeneratorMessenger> fGeneratorMessenger;
  mutable std::atomic_bool fGeneratorFailed{false};
};
}
