/// \file src/generator/GeneratorManager.cc
/// \brief Implementation: Provide the currently implemented particle-gun generation path.
///
/// PrimaryGeneratorAction owns this manager and calls Generate(event).
/// Only the gun selection is implemented; configure it using /gun/ commands.
/// Selection and verbosity references must outlive the manager.

#include "generator/GeneratorManager.hh"

#include "G4Event.hh"
#include "G4Gamma.hh"
#include "G4ParticleDefinition.hh"
#include "G4ParticleGun.hh"
#include "G4PrimaryParticle.hh"
#include "G4PrimaryVertex.hh"
#include "G4SystemOfUnits.hh"
#include "G4ios.hh"

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace cupid {
GeneratorManager::GeneratorManager(const G4String& selection, const G4int& verbosity)
    : fSelection(selection), fVerbosity(verbosity), fGun(std::make_unique<G4ParticleGun>(1)) {
  fGun->SetParticleDefinition(G4Gamma::GammaDefinition());
  fGun->SetParticleEnergy(1.*MeV);
  fGun->SetParticlePosition({0., -350.*mm, -100.*mm});
  fGun->SetParticleMomentumDirection({0., 0., 1.});
}
GeneratorManager::~GeneratorManager() = default;

void GeneratorManager::Check(const G4String& selection) {
  if (selection != "gun")
    throw std::runtime_error("Unsupported generator: " + selection);
}

void GeneratorManager::List(const G4String& selection) {
  Check(selection);
  G4cout << "CUPID_GENERATOR selected=" << selection << " configuration=/gun/" << G4endl;
}

void GeneratorManager::Generate(G4Event* event) {
  Check(fSelection);
  fGun->GeneratePrimaryVertex(event);
  if (fVerbosity > 0) {
    auto* vertex = event->GetPrimaryVertex();
    auto* particle = vertex->GetPrimary();
    std::ostringstream line;
    line << std::setprecision(12) << "CUPID_PRIMARY event=" << event->GetEventID()
         << " source=" << fSelection << " particle=" << particle->GetG4code()->GetParticleName()
         << " x_mm=" << vertex->GetX0()/mm << " y_mm=" << vertex->GetY0()/mm
         << " z_mm=" << vertex->GetZ0()/mm << " kinetic_MeV=" << particle->GetKineticEnergy()/MeV;
    G4cout << line.str() << G4endl;
  }
}
}
