#pragma once
#include "globals.hh"
class G4Material;

namespace cupid {
class Materials {
 public:
  // Call during master-thread geometry initialization, before worker transport.
  // Geant4 owns the elements/materials; repeated calls reuse existing entries.
  void Build();
  // Accepts material names plus WorldAir, LMO_Crystal and CUPID.LMO.Natural aliases.
  G4Material* Get(const G4String& name) const;
};
}

