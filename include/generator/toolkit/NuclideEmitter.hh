#pragma once

#include "G4ThreeVector.hh"
#include "globals.hh"

class G4Event;
namespace cupid {
// Creates a ground-state primary nuclide at rest with zero charge; decay is handled by physics.
class NuclideEmitter {
 public:
  void Emit(G4Event*, G4int z, G4int a, const G4ThreeVector& position) const;
};
}
