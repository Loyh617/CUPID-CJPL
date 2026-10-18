#pragma once
#include "G4UserRunAction.hh"

namespace cupid {
class RunAction final : public G4UserRunAction {
 public:
  void EndOfRunAction(const G4Run*) override;
};
}

