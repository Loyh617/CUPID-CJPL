#pragma once
#include "G4VUserPrimaryGeneratorAction.hh"
#include <atomic>
#include <memory>
namespace cupid {
struct GeneratorConfig;
class GeneratorManager;
class GeometryManager;
class PrimaryGeneratorAction final : public G4VUserPrimaryGeneratorAction {
 public:
  PrimaryGeneratorAction(const GeneratorConfig&, const GeometryManager&, std::atomic_bool& failed);
  ~PrimaryGeneratorAction() override;
  void GeneratePrimaries(G4Event*) override;

 private:
  std::unique_ptr<GeneratorManager> fGenerator;
  std::atomic_bool& fFailed;
};
}
