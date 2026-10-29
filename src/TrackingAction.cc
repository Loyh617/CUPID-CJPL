/// \file src/TrackingAction.cc
/// \brief Implementation: Register track ancestry for per-incident-particle energy reconstruction.
///
/// ActionInitialization supplies EventAction's event-local TrackAncestry and
/// the incident-recording flag. PreUserTrackingAction() registers each track
/// once when enabled; PostUserTrackingAction() is currently a placeholder.

#include "TrackingAction.hh"
#include "record/TrackAncestry.hh"

#include "G4ParticleDefinition.hh"
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
  fAncestry.RegisterTrack(track->GetTrackID(), track->GetParentID(), volume->GetName(),
                          track->GetDefinition()->GetPDGEncoding());
}

void TrackingAction::PostUserTrackingAction(const G4Track*) {
  // Reserved for future implementation.
}

}  // namespace cupid
