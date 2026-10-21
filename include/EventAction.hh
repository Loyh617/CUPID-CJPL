#pragma once
#include "G4UserEventAction.hh"

namespace cupid {
class EventAction final : public G4UserEventAction {
 public:
  void BeginOfEventAction(const G4Event*) override;
  void EndOfEventAction(const G4Event*) override;
};
}
