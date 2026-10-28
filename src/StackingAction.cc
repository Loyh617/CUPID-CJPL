/// \file src/StackingAction.cc
/// \brief Implementation: Provide track-stacking hooks while retaining all tracks in the event.
///
/// Register through ActionInitialization. Geant4 calls ClassifyNewTrack(),
/// which returns fUrgent; NewStage() and PrepareNewEvent() are placeholders.
/// No time-window splitting, track filtering or postponement is applied.

#include "StackingAction.hh"

namespace cupid {
G4ClassificationOfNewTrack StackingAction::ClassifyNewTrack(const G4Track*) {
  // Keep all tracks in the current event; apply no filtering or postponement.
  return fUrgent;
}

void StackingAction::NewStage() {
  // Reserved for future implementation.
}

void StackingAction::PrepareNewEvent() {
  // Reserved for future implementation.
}

}
