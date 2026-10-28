/// \file include/generator/GeneratorManager.hh
/// \brief Declaration: Provide the currently implemented particle-gun generation path.
///
/// PrimaryGeneratorAction owns this manager and calls Generate(event).
/// Only the gun selection is implemented; configure it using /gun/ commands.
/// Selection and verbosity references must outlive the manager.

#pragma once

#include "globals.hh"

#include <memory>

class G4Event;
class G4ParticleGun;
namespace cupid {
class GeneratorManager {
 public:
  GeneratorManager(const G4String& selection, const G4int& verbosity);
  ~GeneratorManager();
  void Generate(G4Event*);
  static void Check(const G4String& selection);
  static void List(const G4String& selection);

 private:
  const G4String& fSelection;
  const G4int& fVerbosity;
  std::unique_ptr<G4ParticleGun> fGun;
};
}
