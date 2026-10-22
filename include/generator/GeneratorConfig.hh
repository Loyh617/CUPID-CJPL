#pragma once
#include "G4ThreeVector.hh"
#include "G4SystemOfUnits.hh"
#include "globals.hh"

namespace cupid {
// Modified only by the master UI in PreInit; read-only during event processing.
struct GeneratorConfig {
  G4String selection = "gun";
  G4String positionMode = "point";
  G4ThreeVector point{0., -350.*mm, -100.*mm};
  G4String volume;
  G4int verbosity = 0;
};
}
