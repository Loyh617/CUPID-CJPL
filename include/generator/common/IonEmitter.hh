#pragma once
#include "G4ThreeVector.hh"
#include "globals.hh"
class G4Event;
namespace cupid {
class IonEmitter {
 public:
  void Emit(G4Event*, G4int z, G4int a, const G4ThreeVector& position) const;
};
}
