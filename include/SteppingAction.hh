#pragma once
#include "G4UserSteppingAction.hh"

namespace cupid {
class SteppingAction final : public G4UserSteppingAction {
 public:
  void UserSteppingAction(const G4Step*) override;
};
}
