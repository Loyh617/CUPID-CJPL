/// \file include/TrackingAction.hh
/// \brief Declaration: Register track ancestry for per-incident-particle energy reconstruction.
///
/// ActionInitialization supplies EventAction's event-local TrackAncestry and
/// the incident-recording flag. PreUserTrackingAction() registers each track
/// once when enabled; PostUserTrackingAction() is currently a placeholder.

#pragma once

#include "G4UserTrackingAction.hh"
#include "globals.hh"

namespace cupid {
class TrackAncestry;

class TrackingAction final : public G4UserTrackingAction {
 public:
  explicit TrackingAction(TrackAncestry& ancestry, G4bool enabled = true);
  void PreUserTrackingAction(const G4Track*) override;
  void PostUserTrackingAction(const G4Track*) override;

 private:
  TrackAncestry& fAncestry;
  const G4bool fEnabled;
};

}  // namespace cupid
