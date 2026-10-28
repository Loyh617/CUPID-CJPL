/// \file include/generator/toolkit/NuclideEmitter.hh
/// \brief Declaration: Add a neutral, ground-state primary nuclide at rest to an event.
///
/// A concrete generator calls Emit(event, Z, A, worldPosition) after particle
/// initialization, using Geant4 units. The helper creates a vertex at time
/// zero; registered physics handles subsequent decay and transport.

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
