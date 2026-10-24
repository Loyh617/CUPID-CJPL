#pragma once

#include "G4UImessenger.hh"

#include <memory>

class G4UIdirectory;
class G4UIcmdWithAString;
class G4UIcmdWithAnInteger;
class G4UIcmdWithoutParameter;
namespace cupid {
class GeneratorMessenger final : public G4UImessenger {
 public:
  GeneratorMessenger(G4String& selection, G4int& verbosity);
  ~GeneratorMessenger() override;
  void SetNewValue(G4UIcommand*, G4String) override;

 private:
  G4String& fSelection;
  G4int& fVerbosity;
  std::unique_ptr<G4UIdirectory> fDirectory;
  std::unique_ptr<G4UIcmdWithAString> fSelect;
  std::unique_ptr<G4UIcmdWithAnInteger> fVerbose;
  std::unique_ptr<G4UIcmdWithoutParameter> fList, fCheck;
};
}
