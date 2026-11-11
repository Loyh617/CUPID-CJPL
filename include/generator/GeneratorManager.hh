/// \file include/generator/GeneratorManager.hh
/// \brief Declaration: Dispatch the selected primary generator on this worker.
///
/// PrimaryGeneratorAction owns this manager and calls Generate(event).
/// Select the particle gun or an LMO Mo-100 decay in the run macro.
/// Selection and verbosity references must outlive the manager.

#pragma once

#include "globals.hh"

#include <memory>

class G4Event;
class G4ParticleGun;
namespace cupid {
class GeometryManager;
class Mo100DoubleBetaDecayGenerator;
class GeneratorManager {
 public:
  GeneratorManager(const G4String& selection, const G4int& verbosity, const GeometryManager& geometry);
  ~GeneratorManager();
  void Generate(G4Event*);
  static void Check(const G4String& selection);
  static void List(const G4String& selection);

 private:
  const G4String& fSelection;
  const G4int& fVerbosity;
  const GeometryManager& fGeometry;
  std::unique_ptr<Mo100DoubleBetaDecayGenerator> fDoubleBeta;
  std::unique_ptr<G4ParticleGun> fGun;
};
}
