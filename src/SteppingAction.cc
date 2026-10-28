/// \file src/SteppingAction.cc
/// \brief Implementation: Reserve a Geant4 stepping hook for future application-specific behavior.
///
/// ActionInitialization registers this action and Geant4 calls
/// UserSteppingAction() after steps. The hook is currently empty; LMO energy
/// deposits are collected by LMOSensitiveDetector, not this action.

#include "SteppingAction.hh"

namespace cupid {
void SteppingAction::UserSteppingAction(const G4Step*) {
  // Reserved for future implementation.
}

}
