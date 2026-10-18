#pragma once

#include "G4VModularPhysicsList.hh"

namespace cupid {

class PhysicsList final : public G4VModularPhysicsList {
 public:
  explicit PhysicsList(G4int verbosity = 1);
  ~PhysicsList() override = default;
};

}  // namespace cupid
