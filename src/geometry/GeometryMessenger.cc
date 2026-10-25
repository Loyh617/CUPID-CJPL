#include "geometry/GeometryMessenger.hh"
#include "geometry/GeometryManager.hh"
#include "G4UIdirectory.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithAnInteger.hh"
#include "G4UIcmdWithoutParameter.hh"
#include "G4Exception.hh"
#include <stdexcept>

namespace cupid {
GeometryMessenger::GeometryMessenger(GeometryManager& manager) : fManager(manager) {
  fRoot = std::make_unique<G4UIdirectory>("/cupid/");
  fDirectory = std::make_unique<G4UIdirectory>("/cupid/geometry/");
  fDirectory->SetGuidance("Select prototype components before /run/initialize.");
  fEnable = std::make_unique<G4UIcmdWithAString>("/cupid/geometry/enable", this);
  fDisable = std::make_unique<G4UIcmdWithAString>("/cupid/geometry/disable", this);
  for (auto* command : {fEnable.get(), fDisable.get()}) {
    command->SetParameterName("component", false);
    command->SetGuidance("Component: Cryostat, InnerShielding or LMOArray (case sensitive).");
    command->AvailableForStates(G4State_PreInit);
    command->SetToBeBroadcasted(false);
  }
  fList = std::make_unique<G4UIcmdWithoutParameter>("/cupid/geometry/list", this);
  fList->AvailableForStates(G4State_PreInit, G4State_Idle);
  fList->SetToBeBroadcasted(false);
  fCount = std::make_unique<G4UIcmdWithAnInteger>("/cupid/geometry/lmoCount", this);
  fCount->SetParameterName("count", false);
  fCount->SetRange("count>=1 && count<=2");
  fCount->SetGuidance("One centered crystal or two along X; does not enable LMOArray.");
  fCount->AvailableForStates(G4State_PreInit);
  fCount->SetToBeBroadcasted(false);
  fCheck = std::make_unique<G4UIcmdWithAnInteger>("/cupid/geometry/check", this);
  fCheck->SetParameterName("samples", true);
  fCheck->SetDefaultValue(10000);
  fCheck->SetRange("samples>=100");
  fCheck->SetGuidance("Check each placed volume; fail the macro if overlaps are found.");
  fCheck->AvailableForStates(G4State_Idle);
  fCheck->SetToBeBroadcasted(false);
}
GeometryMessenger::~GeometryMessenger() = default;
void GeometryMessenger::SetNewValue(G4UIcommand* command, G4String value) {
  try {
    if (command == fEnable.get()) fManager.SetEnabled(value, true);
    else if (command == fDisable.get()) fManager.SetEnabled(value, false);
    else if (command == fList.get()) fManager.List();
    else if (command == fCount.get()) fManager.SetLMOCount(fCount->GetNewIntValue(value));
    else if (command == fCheck.get() && !fManager.CheckOverlaps(fCheck->GetNewIntValue(value))) {
      throw std::runtime_error("Geometry overlap check failed. Inspect the reported volumes.");
    }
  } catch (const std::exception& error) {
    G4ExceptionDescription description;
    description << error.what();
    command->CommandFailed(description);
  }
}
}  // namespace cupid

