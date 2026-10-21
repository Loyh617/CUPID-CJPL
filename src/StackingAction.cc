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
