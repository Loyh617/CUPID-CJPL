/// \file src/generator/GeneratorManager.cc
/// \brief Implementation: Dispatch the selected primary generator on this worker.
///
/// PrimaryGeneratorAction owns this manager and calls Generate(event).
/// Select the particle gun or an LMO Mo-100 decay in the run macro.
/// Selection and verbosity references must outlive the manager.

#include "generator/GeneratorManager.hh"
#include "generator/lmo_signal/double_beta_decay/Mo100DoubleBetaDecayGenerator.hh"

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
GeneratorManager::GeneratorManager(const G4String& selection, const G4int& verbosity, const GeometryManager& geometry)
    : fSelection(selection), fVerbosity(verbosity), fGeometry(geometry), fGun(std::make_unique<G4ParticleGun>(1)) {
  // Default source: one 1 MeV gamma per event. Override after /run/initialize
  // using /gun/number, particle, energy, position and direction in the run macro.
  // Position is in World coordinates; direction is the particle momentum direction.
  fGun->SetParticleDefinition(G4Gamma::GammaDefinition());
  fGun->SetParticleEnergy(1.*MeV);
  fGun->SetParticlePosition({0., -350.*mm, -100.*mm});
  fGun->SetParticleMomentumDirection({0., 0., 1.});
}
GeneratorManager::~GeneratorManager() = default;

void GeneratorManager::Check(const G4String& selection) {
  if (selection != "gun" && selection != "Mo100TwoNu" && selection != "Mo100ZeroNu")
    throw std::runtime_error("Unsupported generator: " + selection);
}

void GeneratorManager::List(const G4String& selection) {
  Check(selection);
  G4cout << "CUPID_GENERATOR selected=" << selection << (selection == "gun" ? " configuration=/gun/" :
      selection == "Mo100TwoNu" ? " model=allowed-phase-space Q_keV=3034.40" :
                                  " model=light-Majorana-mass Q_keV=3034.40") << G4endl;
}

void GeneratorManager::Generate(G4Event* event) {
  Check(fSelection);
  if (fSelection == "gun") {
    fGun->GeneratePrimaryVertex(event);
  } else {
    if (!fDoubleBeta) {
      using Mode = Mo100DoubleBetaDecayGenerator::Mode;
      fDoubleBeta = std::make_unique<Mo100DoubleBetaDecayGenerator>(
          fGeometry, fSelection == "Mo100TwoNu" ? Mode::TwoNu : Mode::ZeroNu);
    }
    fDoubleBeta->Generate(event);
  }
  if (fVerbosity > 0) {
    auto* vertex = event->GetPrimaryVertex();
    auto* particle = vertex->GetPrimary();
    std::ostringstream line;
    line << std::setprecision(12) << "CUPID_PRIMARY event=" << event->GetEventID()
         << " source=" << fSelection << " particle=" << particle->GetG4code()->GetParticleName()
         << " x_mm=" << vertex->GetX0()/mm << " y_mm=" << vertex->GetY0()/mm
         << " z_mm=" << vertex->GetZ0()/mm << " kinetic_MeV=" << particle->GetKineticEnergy()/MeV;
    if (fDoubleBeta) {
      const auto* other = particle->GetNext();
      line << " second_kinetic_MeV=" << other->GetKineticEnergy()/MeV
           << " cos12=" << particle->GetMomentumDirection().dot(other->GetMomentumDirection());
    }
    G4cout << line.str() << G4endl;
  }
}
}
