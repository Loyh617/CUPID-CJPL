#include "generator/GeneratorMessenger.hh"
#include "generator/GeneratorManager.hh"

#include "G4Exception.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithAnInteger.hh"
#include "G4UIcmdWithoutParameter.hh"
#include "G4UIdirectory.hh"
#include "G4ios.hh"

#include <exception>

namespace cupid {
GeneratorMessenger::GeneratorMessenger(G4String& selection, G4int& verbosity)
    : fSelection(selection), fVerbosity(verbosity) {
  fDirectory = std::make_unique<G4UIdirectory>("/cupid/generator/");
  fDirectory->SetGuidance("Select an implemented generator; sampling belongs to each concrete generator.");
  fSelect = std::make_unique<G4UIcmdWithAString>("/cupid/generator/select", this);
  fSelect->SetParameterName("source", false);
  fSelect->SetCandidates("gun");
  fSelect->SetGuidance("Only gun is currently implemented. Configure it with /gun/ commands.");
  fVerbose = std::make_unique<G4UIcmdWithAnInteger>("/cupid/generator/verbose", this);
  fVerbose->SetParameterName("level", false);
  fVerbose->SetRange("level>=0 && level<=1");
  fVerbose->SetGuidance("0: silent; 1: primary diagnostic line for every event (small tests only).");
  for (G4UIcommand* command : {static_cast<G4UIcommand*>(fSelect.get()),
                               static_cast<G4UIcommand*>(fVerbose.get())}) {
    command->AvailableForStates(G4State_PreInit);
    command->SetToBeBroadcasted(false);
  }
  fList = std::make_unique<G4UIcmdWithoutParameter>("/cupid/generator/list", this);
  fList->AvailableForStates(G4State_PreInit, G4State_Idle);
  fList->SetToBeBroadcasted(false);
  fCheck = std::make_unique<G4UIcmdWithoutParameter>("/cupid/generator/check", this);
  fCheck->SetGuidance("Check generator selection only; does not validate /gun/ positions or physics.");
  fCheck->AvailableForStates(G4State_Idle);
  fCheck->SetToBeBroadcasted(false);
}
GeneratorMessenger::~GeneratorMessenger() = default;

void GeneratorMessenger::SetNewValue(G4UIcommand* command, G4String value) {
  try {
    if (command == fSelect.get()) {
      GeneratorManager::Check(value);
      fSelection = value;
    } else if (command == fVerbose.get()) {
      fVerbosity = fVerbose->GetNewIntValue(value);
    } else if (command == fList.get()) {
      GeneratorManager::List(fSelection);
    } else if (command == fCheck.get()) {
      GeneratorManager::Check(fSelection);
      G4cout << "CUPID_GENERATOR_CHECK scope=selection result=PASS" << G4endl;
    }
  } catch (const std::exception& error) {
    G4ExceptionDescription description;
    description << error.what();
    command->CommandFailed(description);
  }
}
}
