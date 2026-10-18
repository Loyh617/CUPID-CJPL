#include "geometry/GeometryMessenger.hh"
#include "geometry/GeometryManager.hh"
#include "G4UIdirectory.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithAnInteger.hh"
#include "G4UIcmdWithoutParameter.hh"
#include "G4Exception.hh"
#include <stdexcept>

namespace cupid {
GeometryMessenger::GeometryMessenger(GeometryManager& manager) : manager_(manager) {
  root_ = std::make_unique<G4UIdirectory>("/cupid/");
  directory_ = std::make_unique<G4UIdirectory>("/cupid/geometry/");
  directory_->SetGuidance("Select prototype components before /run/initialize.");
  enable_ = std::make_unique<G4UIcmdWithAString>("/cupid/geometry/enable", this);
  disable_ = std::make_unique<G4UIcmdWithAString>("/cupid/geometry/disable", this);
  for (auto* command : {enable_.get(), disable_.get()}) {
    command->SetParameterName("component", false);
    command->SetGuidance("Component: Cryostat, InnerShielding or CrystalArray (case sensitive).");
    command->AvailableForStates(G4State_PreInit);
    command->SetToBeBroadcasted(false);
  }
  list_ = std::make_unique<G4UIcmdWithoutParameter>("/cupid/geometry/list", this);
  list_->AvailableForStates(G4State_PreInit, G4State_Idle);
  list_->SetToBeBroadcasted(false);
  count_ = std::make_unique<G4UIcmdWithAnInteger>("/cupid/geometry/crystalCount", this);
  count_->SetParameterName("count", false);
  count_->SetRange("count>=1 && count<=2");
  count_->SetGuidance("One centered crystal or two along X; does not enable CrystalArray.");
  count_->AvailableForStates(G4State_PreInit);
  count_->SetToBeBroadcasted(false);
  check_ = std::make_unique<G4UIcmdWithAnInteger>("/cupid/geometry/check", this);
  check_->SetParameterName("samples", true);
  check_->SetDefaultValue(10000);
  check_->SetRange("samples>=100");
  check_->SetGuidance("Check each placed volume; fail the macro if overlaps are found.");
  check_->AvailableForStates(G4State_Idle);
  check_->SetToBeBroadcasted(false);
}
GeometryMessenger::~GeometryMessenger() = default;
void GeometryMessenger::SetNewValue(G4UIcommand* command, G4String value) {
  try {
    if (command == enable_.get()) manager_.SetEnabled(value, true);
    else if (command == disable_.get()) manager_.SetEnabled(value, false);
    else if (command == list_.get()) manager_.List();
    else if (command == count_.get()) manager_.SetCrystalCount(count_->GetNewIntValue(value));
    else if (command == check_.get() && !manager_.CheckOverlaps(check_->GetNewIntValue(value))) {
      throw std::runtime_error("Geometry overlap check failed. Inspect the reported volumes.");
    }
  } catch (const std::exception& error) {
    G4ExceptionDescription description;
    description << error.what();
    command->CommandFailed(description);
  }
}
}  // namespace cupid

