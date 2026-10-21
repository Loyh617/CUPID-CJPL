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
