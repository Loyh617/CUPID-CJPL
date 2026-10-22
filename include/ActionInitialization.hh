#pragma once
#include "G4VUserActionInitialization.hh"
#include "generator/GeneratorConfig.hh"
#include <atomic>
#include <memory>

namespace cupid {
class GeneratorMessenger;
class ActionInitialization final : public G4VUserActionInitialization {
 public:
  ActionInitialization();
  ~ActionInitialization() override;
  void BuildForMaster() const override;
  void Build() const override;
  bool HasGeneratorError() const { return fGeneratorFailed.load(); }

 private:
  GeneratorConfig fGeneratorConfig;
  std::unique_ptr<GeneratorMessenger> fGeneratorMessenger;
  mutable std::atomic_bool fGeneratorFailed{false};
};
}
