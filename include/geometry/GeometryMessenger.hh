/// \file include/geometry/GeometryMessenger.hh
/// \brief Declaration: Translate geometry macro commands into GeometryManager operations.
///
/// GeometryManager owns this messenger. Before /run/initialize, use
/// /cupid/geometry/enable, disable and lmoCount to configure components.
/// Use /cupid/geometry/check after initialization to check placed volumes.

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
  GeometryManager& fManager;
  std::unique_ptr<G4UIdirectory> fRoot;
  std::unique_ptr<G4UIdirectory> fDirectory;
  std::unique_ptr<G4UIcmdWithAString> fEnable;
  std::unique_ptr<G4UIcmdWithAString> fDisable;
  std::unique_ptr<G4UIcmdWithoutParameter> fList;
  std::unique_ptr<G4UIcmdWithAnInteger> fCount;
  std::unique_ptr<G4UIcmdWithAnInteger> fCheck;
};
}

