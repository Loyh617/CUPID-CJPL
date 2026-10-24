#pragma once

#include "G4VUserPrimaryGeneratorAction.hh"
#include "globals.hh"

#include <atomic>
#include <memory>

namespace cupid {
class GeneratorManager;
class PrimaryGeneratorAction final : public G4VUserPrimaryGeneratorAction {
 public:
  PrimaryGeneratorAction(const G4String& selection, const G4int& verbosity, std::atomic_bool& failed);
  ~PrimaryGeneratorAction() override;
  void GeneratePrimaries(G4Event*) override;

 private:
  std::unique_ptr<GeneratorManager> fGenerator;
  std::atomic_bool& fFailed;
};
}
