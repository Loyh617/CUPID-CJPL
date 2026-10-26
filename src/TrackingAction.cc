#include "TrackingAction.hh"
#include "record/TrackAncestry.hh"

#include "G4Track.hh"
#include "G4VPhysicalVolume.hh"

#include <stdexcept>

namespace cupid {

TrackingAction::TrackingAction(TrackAncestry& ancestry, G4bool enabled)
    : fAncestry(ancestry), fEnabled(enabled) {}

void TrackingAction::PreUserTrackingAction(const G4Track* track) {
  if (!fEnabled) return;
  // Resumed tracks retain the original birth volume recorded on their first callback.
  if (fAncestry.Contains(track->GetTrackID())) return;
  const auto* volume = track->GetVolume();
  if (!volume) throw std::runtime_error("Cannot register a track without its initial volume.");
  fAncestry.RegisterTrack(track->GetTrackID(), track->GetParentID(), volume->GetName());
}

void TrackingAction::PostUserTrackingAction(const G4Track*) {
  // Reserved for future implementation.
}

}  // namespace cupid
