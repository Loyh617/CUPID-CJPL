/// \file include/PhysicsList.hh
/// \brief Declaration: Assemble the modular decay, electromagnetic and nuclear transport physics.
///
/// Register PhysicsList with the run manager in main() before initialization.
/// Its constructor registers individual physics constructors and configures
/// atomic relaxation, radioactive-decay time limits and production cuts.
/// Double-beta primary kinematics must be supplied by a generator.

#pragma once

#include "G4VModularPhysicsList.hh"

namespace cupid {

class PhysicsList final : public G4VModularPhysicsList {
 public:
  explicit PhysicsList(G4int verbosity = 1);
  ~PhysicsList() override = default;
};

}  // namespace cupid
