#pragma once
#include "globals.hh"
class G4Material;

namespace cupid {
class Materials {
 public:
  // Call during master-thread geometry initialization, before worker transport.
  // Geant4 owns the elements/materials; repeated calls reuse existing entries.
  void Build();
  // Requires the exact G4Material name; unknown names throw an exception.
  G4Material* Get(const G4String& name) const;
};
}

