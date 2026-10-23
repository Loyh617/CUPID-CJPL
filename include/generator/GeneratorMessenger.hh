#pragma once
#include "G4UImessenger.hh"
#include <memory>
class G4UIdirectory;
class G4UIcmdWithAString;
class G4UIcmdWith3VectorAndUnit;
class G4UIcmdWithAnInteger;
class G4UIcmdWithoutParameter;
namespace cupid {
struct GeneratorConfig;
class GeometryManager;
class GeneratorMessenger final : public G4UImessenger {
 public:
  GeneratorMessenger(GeneratorConfig&, const GeometryManager&);
  ~GeneratorMessenger() override;
  void SetNewValue(G4UIcommand*, G4String) override;

 private:
  const GeometryManager& fGeometry;
  GeneratorConfig& fConfig;
  std::unique_ptr<G4UIdirectory> fDirectory;
  std::unique_ptr<G4UIcmdWithAString> fSelect, fPosition, fVolume;
  std::unique_ptr<G4UIcmdWith3VectorAndUnit> fPoint;
  std::unique_ptr<G4UIcmdWithAnInteger> fVerbose;
  std::unique_ptr<G4UIcmdWithoutParameter> fList, fCheck;
};
}
