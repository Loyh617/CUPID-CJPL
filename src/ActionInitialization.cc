#include "ActionInitialization.hh"
#include "PrimaryGeneratorAction.hh"
#include "RunAction.hh"

namespace cupid {
void ActionInitialization::BuildForMaster() const {
  SetUserAction(new RunAction);
}
void ActionInitialization::Build() const {
  SetUserAction(new PrimaryGeneratorAction);
  SetUserAction(new RunAction);
}
}

