#include "generator/toolkit/NuclideEmitter.hh"

#include "G4Event.hh"
#include "G4IonTable.hh"
#include "G4PrimaryParticle.hh"
#include "G4PrimaryVertex.hh"

#include <memory>
#include <stdexcept>

namespace cupid {
void NuclideEmitter::Emit(G4Event* event, G4int z, G4int a,
                      const G4ThreeVector& position) const {
  auto* ion = G4IonTable::GetIonTable()->GetIon(z, a, 0.);
  if (!ion) throw std::runtime_error("Requested ground-state ion is unavailable.");
  auto vertex = std::make_unique<G4PrimaryVertex>(position, 0.);
  auto particle = std::make_unique<G4PrimaryParticle>(ion);
  particle->SetKineticEnergy(0.);
  particle->SetCharge(0.);
  vertex->SetPrimary(particle.release());
  event->AddPrimaryVertex(vertex.release());
}
}
