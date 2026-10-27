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
  SetUserAction(new RunAction(fOutputDirectory, false));
}
void ActionInitialization::Build() const {
  SetUserAction(new PrimaryGeneratorAction(fGeneratorSelection, fGeneratorVerbosity, fGeneratorFailed));
  auto* runAction = new RunAction(fOutputDirectory);
  SetUserAction(runAction);
  auto* eventAction = new EventAction(runAction);
  SetUserAction(eventAction);
  SetUserAction(new SteppingAction);
  SetUserAction(new TrackingAction(eventAction->GetTrackAncestry(),
                                   eventAction->IsIncidentRecordingEnabled()));
  SetUserAction(new StackingAction);
}
}

