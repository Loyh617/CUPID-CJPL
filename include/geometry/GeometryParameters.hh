#pragma once
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include <array>

namespace cupid {
// Prototype dimensions inferred from reference directory "1".
// Coordinates are right-handed; the cryostat axis is +Y.
// The environment is deliberately vacuum, not a model of the laboratory.
struct GeometryParameters {
  G4double worldHalfLength = 2.0 * m;
  G4int lmoCount = 1;
  G4double lmoSide = 45.0 * mm;
  G4double lmoGap = 5.0 * mm;
  G4ThreeVector lmoCenter{0., -350.0 * mm, 0.};

  struct Plate {
    const char* name;
    const char* material;
    G4double radius;
    G4double bottomY;
    G4double thickness;
    G4bool serviceHoles;
  };
  // MXC is a flat plate in v0.1: bosses, recesses and posts are omitted.
  const std::array<Plate, 6> plates{{
      {"MXC", "G4_Cu", 250.*mm, 0.*mm, 8.*mm, false},
      {"HEX", "G4_Cu", 255.*mm, 170.*mm, 6.*mm, true},
      {"Still", "G4_Cu", 270.*mm, 266.*mm, 9.*mm, true},
      {"4K", "G4_Cu", 295.*mm, 515.*mm, 10.*mm, true},
      {"50K", "G4_STAINLESS-STEEL", 330.*mm, 802.*mm, 14.*mm, true},
      {"300K", "G4_STAINLESS-STEEL", 432.5*mm, 983.7*mm, 28.*mm, true}
  }};

  struct ShieldPart {
    const char* name;
    G4double innerRadius;
    G4double height;
    G4double centerY;
  };
  G4double shieldOuterRadius = 160.*mm;
  const std::array<ShieldPart, 3> innerShield{{
      {"Upper", 15.*mm, 20.*mm, -310.*mm},
      {"Body", 60.*mm, 100.*mm, -370.*mm},
      {"Lower", 0.*mm, 20.*mm, -430.*mm}
  }};
};
}

