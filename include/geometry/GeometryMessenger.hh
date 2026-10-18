#pragma once
#include "G4UImessenger.hh"
#include <memory>

class G4UIdirectory;
class G4UIcmdWithAString;
class G4UIcmdWithAnInteger;
class G4UIcmdWithoutParameter;

namespace cupid {
class GeometryManager;
class GeometryMessenger final : public G4UImessenger {
 public:
  explicit GeometryMessenger(GeometryManager& manager);
  ~GeometryMessenger() override;
  void SetNewValue(G4UIcommand*, G4String) override;

 private:
  GeometryManager& manager_;
  std::unique_ptr<G4UIdirectory> root_;
  std::unique_ptr<G4UIdirectory> directory_;
  std::unique_ptr<G4UIcmdWithAString> enable_;
  std::unique_ptr<G4UIcmdWithAString> disable_;
  std::unique_ptr<G4UIcmdWithoutParameter> list_;
  std::unique_ptr<G4UIcmdWithAnInteger> count_;
  std::unique_ptr<G4UIcmdWithAnInteger> check_;
};
}

