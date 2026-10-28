/// \file include/SteppingAction.hh
/// \brief Declaration: Reserve a Geant4 stepping hook for future application-specific behavior.
///
/// ActionInitialization registers this action and Geant4 calls
/// UserSteppingAction() after steps. The hook is currently empty; LMO energy
/// deposits are collected by LMOSensitiveDetector, not this action.

#pragma once
#include "G4UserSteppingAction.hh"

namespace cupid {
class SteppingAction final : public G4UserSteppingAction {
 public:
  void UserSteppingAction(const G4Step*) override;
};
}
