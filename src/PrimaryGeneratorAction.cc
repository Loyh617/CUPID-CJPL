#include "PrimaryGeneratorAction.hh"
#include "generator/GeneratorManager.hh"
#include "G4RunManager.hh"
#include "G4ios.hh"
#include <exception>

namespace cupid {
PrimaryGeneratorAction::PrimaryGeneratorAction(const GeneratorConfig& config,
                                               std::atomic_bool& failed)
    : fGenerator(std::make_unique<GeneratorManager>(config)), fFailed(failed) {}
PrimaryGeneratorAction::~PrimaryGeneratorAction() = default;
void PrimaryGeneratorAction::GeneratePrimaries(G4Event* event) {
  if (fFailed.load()) {
    G4RunManager::GetRunManager()->AbortRun(true);
    return;
  }
  try {
    fGenerator->Generate(event);
  } catch (const std::exception& error) {
    if (!fFailed.exchange(true)) G4cerr << "CUPID_GENERATOR_ERROR: " << error.what() << G4endl;
    G4RunManager::GetRunManager()->AbortRun(true);
  }
}
}
