#pragma once
#include "G4UserTrackingAction.hh"

namespace cupid {
class TrackingAction final : public G4UserTrackingAction {
 public:
  void PreUserTrackingAction(const G4Track*) override;
  void PostUserTrackingAction(const G4Track*) override;
};
}
