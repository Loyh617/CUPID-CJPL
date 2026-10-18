#pragma once
#include "G4VUserPrimaryGeneratorAction.hh"
#include <memory>

class G4ParticleGun;
namespace cupid {
class PrimaryGeneratorAction final : public G4VUserPrimaryGeneratorAction {
 public:
  PrimaryGeneratorAction();
  ~PrimaryGeneratorAction() override;
  void GeneratePrimaries(G4Event*) override;

 private:
  std::unique_ptr<G4ParticleGun> gun_;
};
}

