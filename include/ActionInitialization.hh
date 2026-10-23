#pragma once
#include "G4VUserActionInitialization.hh"
#include "generator/GeneratorConfig.hh"
#include <atomic>
#include <memory>

namespace cupid {
class GeneratorMessenger;
class GeometryManager;
class ActionInitialization final : public G4VUserActionInitialization {
 public:
  explicit ActionInitialization(const GeometryManager& geometry);
  ~ActionInitialization() override;
  void BuildForMaster() const override;
  void Build() const override;
  bool HasGeneratorError() const { return fGeneratorFailed.load(); }

 private:
  const GeometryManager& fGeometry;
  GeneratorConfig fGeneratorConfig;
  std::unique_ptr<GeneratorMessenger> fGeneratorMessenger;
  mutable std::atomic_bool fGeneratorFailed{false};
};
}
