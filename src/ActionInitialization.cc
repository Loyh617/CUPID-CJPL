/// \file src/ActionInitialization.cc
/// \brief Implementation: Connect the Geant4 user actions and their per-thread dependencies.
///
/// Register with the run manager in main(). Geant4 calls Build() for event
/// processing and BuildForMaster() for the MT coordinator. Each worker owns
/// its actions; EventAction shares that worker's RunAction and track ancestry.

#include "ActionInitialization.hh"
#include "generator/GeneratorMessenger.hh"
#include "geometry/GeometryManager.hh"
#include "PrimaryGeneratorAction.hh"
#include "RunAction.hh"
#include "EventAction.hh"
#include "SteppingAction.hh"
#include "TrackingAction.hh"
#include "StackingAction.hh"

#include "G4LogicalVolume.hh"

namespace cupid {
ActionInitialization::ActionInitialization(const G4String& outputDirectory, const GeometryManager& geometry)
    : fGeometry(geometry), fOutputDirectory(outputDirectory), fGeneratorMessenger(std::make_unique<GeneratorMessenger>(fGeneratorSelection, fGeneratorVerbosity)) {}
ActionInitialization::~ActionInitialization() = default;
RunAction* ActionInitialization::CreateRunAction() const {
  // Resolve the complete scoring catalog after geometry construction, including
  // on the MT master whose RunAction is created before geometry initialization.
  return new RunAction(fOutputDirectory, EventAction::DefaultRecorders(), "RootOutput", [this] {
    std::vector<G4String> names;
    for (const auto* logical : fGeometry.GetLMOLogicalVolumes()) names.push_back(logical->GetName());
    return names;
  });
}
void ActionInitialization::BuildForMaster() const {
  SetUserAction(CreateRunAction());
}
void ActionInitialization::Build() const {
  SetUserAction(new PrimaryGeneratorAction(fGeneratorSelection, fGeneratorVerbosity, fGeneratorFailed));
  auto* runAction = CreateRunAction();
  SetUserAction(runAction);
  auto* eventAction = new EventAction(runAction);
  SetUserAction(eventAction);
  SetUserAction(new SteppingAction);
  SetUserAction(new TrackingAction(eventAction->GetTrackAncestry(),
                                   eventAction->RequiresTrackAncestry()));
  SetUserAction(new StackingAction);
}
}

