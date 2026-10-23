#include "ActionInitialization.hh"
#include "generator/GeneratorMessenger.hh"
#include "PrimaryGeneratorAction.hh"
#include "RunAction.hh"
#include "EventAction.hh"
#include "SteppingAction.hh"
#include "TrackingAction.hh"
#include "StackingAction.hh"

namespace cupid {
ActionInitialization::ActionInitialization(const GeometryManager& geometry)
    : fGeometry(geometry),
      fGeneratorMessenger(std::make_unique<GeneratorMessenger>(fGeneratorConfig, geometry)) {}
ActionInitialization::~ActionInitialization() = default;
void ActionInitialization::BuildForMaster() const {
  SetUserAction(new RunAction);
}
void ActionInitialization::Build() const {
  SetUserAction(new PrimaryGeneratorAction(fGeneratorConfig, fGeometry, fGeneratorFailed));
  SetUserAction(new RunAction);
  SetUserAction(new EventAction);
  SetUserAction(new SteppingAction);
  SetUserAction(new TrackingAction);
  SetUserAction(new StackingAction);
}
}

