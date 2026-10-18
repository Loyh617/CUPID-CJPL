#include "PrimaryGeneratorAction.hh"
#include "G4Gamma.hh"
#include "G4ParticleGun.hh"
#include "G4SystemOfUnits.hh"

namespace cupid {
PrimaryGeneratorAction::PrimaryGeneratorAction()
    : gun_(std::make_unique<G4ParticleGun>(1)) {
  gun_->SetParticleDefinition(G4Gamma::GammaDefinition());
  gun_->SetParticleEnergy(1.*MeV);
  gun_->SetParticlePosition({0., -350.*mm, -100.*mm});
  gun_->SetParticleMomentumDirection({0., 0., 1.});
}
PrimaryGeneratorAction::~PrimaryGeneratorAction() = default;
void PrimaryGeneratorAction::GeneratePrimaries(G4Event* event) {
  gun_->GeneratePrimaryVertex(event);
}
}

