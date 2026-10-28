/// \file src/ActionInitialization.cc
/// \brief Implementation: Connect the Geant4 user actions and their per-thread dependencies.
///
/// Register with the run manager in main(). Geant4 calls Build() for event
/// processing and BuildForMaster() for the MT coordinator. Each worker owns
/// its actions; EventAction shares that worker's RunAction and track ancestry.

#include "ActionInitialization.hh"
#include "generator/GeneratorMessenger.hh"
#include "PrimaryGeneratorAction.hh"
#include "RunAction.hh"
#include "EventAction.hh"
#include "SteppingAction.hh"
#include "TrackingAction.hh"
#include "StackingAction.hh"

namespace cupid {
ActionInitialization::ActionInitialization(const G4String& outputDirectory)
    : fOutputDirectory(outputDirectory), fGeneratorMessenger(std::make_unique<GeneratorMessenger>(fGeneratorSelection, fGeneratorVerbosity)) {}
ActionInitialization::~ActionInitialization() = default;
void ActionInitialization::BuildForMaster() const {
  SetUserAction(new RunAction(fOutputDirectory, EventAction::DefaultRecorders()));
}
void ActionInitialization::Build() const {
  SetUserAction(new PrimaryGeneratorAction(fGeneratorSelection, fGeneratorVerbosity, fGeneratorFailed));
  auto* runAction = new RunAction(fOutputDirectory, EventAction::DefaultRecorders());
  SetUserAction(runAction);
  auto* eventAction = new EventAction(runAction);
  SetUserAction(eventAction);
  SetUserAction(new SteppingAction);
  SetUserAction(new TrackingAction(eventAction->GetTrackAncestry(),
                                   eventAction->RequiresTrackAncestry()));
  SetUserAction(new StackingAction);
}
}

