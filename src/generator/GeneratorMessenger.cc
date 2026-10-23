#include "generator/GeneratorMessenger.hh"
#include "generator/GeneratorConfig.hh"
#include "generator/GeneratorManager.hh"
#include "G4Exception.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWith3VectorAndUnit.hh"
#include "G4UIcmdWithAnInteger.hh"
#include "G4UIcmdWithoutParameter.hh"
#include "G4UIdirectory.hh"
#include "G4ios.hh"
#include <cmath>
#include <stdexcept>

namespace cupid {
GeneratorMessenger::GeneratorMessenger(GeneratorConfig& config, const GeometryManager& geometry)
    : fGeometry(geometry), fConfig(config) {
  fDirectory = std::make_unique<G4UIdirectory>("/cupid/generator/");
  fDirectory->SetGuidance("Configure one source before initialization; calibration ions use full-chain transport.");
  fSelect = std::make_unique<G4UIcmdWithAString>("/cupid/generator/select", this);
  fSelect->SetParameterName("source", false);
  fSelect->SetCandidates("gun calibration/th232 calibration/co56");
  fSelect->SetGuidance("Select one implemented scenario. /gun/ commands affect only gun mode.");
  fPosition = std::make_unique<G4UIcmdWithAString>("/cupid/generator/position", this);
  fPosition->SetParameterName("mode", false);
  fPosition->SetCandidates("point volume");
  fVolume = std::make_unique<G4UIcmdWithAString>("/cupid/generator/volume", this);
  fVolume->SetParameterName("physicalVolume", false);
  fVolume->SetGuidance("Registered unique physical placement name; samples its material excluding daughter volumes.");
  fPoint = std::make_unique<G4UIcmdWith3VectorAndUnit>("/cupid/generator/point", this);
  fPoint->SetParameterName("x", "y", "z", false);
  fPoint->SetUnitCategory("Length");
  fPoint->SetGuidance("Source position in world coordinates; used only in point mode.");
  fVerbose = std::make_unique<G4UIcmdWithAnInteger>("/cupid/generator/verbose", this);
  fVerbose->SetParameterName("level", false);
  fVerbose->SetRange("level>=0 && level<=1");
  fVerbose->SetGuidance("0: silent; 1: primary diagnostic line for every event (small tests only).");
  G4UIcommand* commands[] = {fSelect.get(), fPosition.get(), fVolume.get(), fPoint.get(), fVerbose.get()};
  for (auto* command : commands) {
    command->AvailableForStates(G4State_PreInit);
    command->SetToBeBroadcasted(false);
  }
  fList = std::make_unique<G4UIcmdWithoutParameter>("/cupid/generator/list", this);
  fList->AvailableForStates(G4State_PreInit, G4State_Idle);
  fList->SetToBeBroadcasted(false);
  fCheck = std::make_unique<G4UIcmdWithoutParameter>("/cupid/generator/check", this);
  fCheck->SetGuidance("Validate source geometry without consuming random numbers.");
  fCheck->AvailableForStates(G4State_Idle);
  fCheck->SetToBeBroadcasted(false);
}
GeneratorMessenger::~GeneratorMessenger() = default;
void GeneratorMessenger::SetNewValue(G4UIcommand* command, G4String value) {
  try {
    if (command == fSelect.get()) fConfig.selection = value;
    else if (command == fPosition.get()) fConfig.positionMode = value;
    else if (command == fVolume.get()) fConfig.volume = value;
    else if (command == fVerbose.get()) fConfig.verbosity = fVerbose->GetNewIntValue(value);
    else if (command == fPoint.get()) {
      const auto point = fPoint->GetNew3VectorValue(value);
      if (!std::isfinite(point.x()) || !std::isfinite(point.y()) || !std::isfinite(point.z()))
        throw std::runtime_error("Source point must have finite coordinates.");
      fConfig.point = point;
    } else if (command == fList.get()) GeneratorManager::List(fConfig);
    else if (command == fCheck.get()) {
      GeneratorManager::Check(fConfig, fGeometry);
      G4cout << "CUPID_GENERATOR_CHECK result=PASS" << G4endl;
    }
  } catch (const std::exception& error) {
    G4ExceptionDescription description;
    description << error.what();
    command->CommandFailed(description);
  }
}
}
