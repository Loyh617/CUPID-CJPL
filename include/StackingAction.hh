/// \file include/StackingAction.hh
/// \brief Declaration: Provide track-stacking hooks while retaining all tracks in the event.
///
/// Register through ActionInitialization. Geant4 calls ClassifyNewTrack(),
/// which returns fUrgent; NewStage() and PrepareNewEvent() are placeholders.
/// No time-window splitting, track filtering or postponement is applied.

#pragma once
#include "G4UserStackingAction.hh"

namespace cupid {
class StackingAction final : public G4UserStackingAction {
 public:
  G4ClassificationOfNewTrack ClassifyNewTrack(const G4Track*) override;
  void NewStage() override;
  void PrepareNewEvent() override;
};
}
