#pragma once
#include "G4VUserActionInitialization.hh"

namespace cupid {
class ActionInitialization final : public G4VUserActionInitialization {
 public:
  void BuildForMaster() const override;
  void Build() const override;
};
}

