/// \file include/geometry/GeometryParameters.hh
/// \brief Declaration: Hold prototype dimensions and defaults shared by geometry builders.
///
/// GeometryManager owns the parameters; DetectorConstruction and component
/// Build() methods read them. Specify lengths in Geant4 units; the cryostat
/// axis is Y. These defaults do not define a laboratory or rock model.

#pragma once
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include <array>

namespace cupid {
// Prototype dimensions inferred from reference directory "1".
// Coordinates are right-handed; the cryostat axis is +Y.
// The environment is deliberately vacuum, not a model of the laboratory.
struct GeometryParameters {
  // Source defaults below require recompilation. /cupid/geometry/lmoCount can
  // override the count before initialization; dimensions have no macro override.
  G4double worldHalfLength = 2.0 * m;  // Half side of the vacuum world cube.
  G4int lmoCount = 1;  // Supported: 1 or 2; enabling LMOArray is a separate setting.
  G4double lmoSide = 45.0 * mm;  // Full crystal side length.
  G4double lmoGap = 5.0 * mm;  // Surface-to-surface gap along the array X axis.
  // Array center in World coordinates; the cryostat axis is Y.
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
  // Entries: name, exact material name, radius, bottom Y, full thickness, service holes.
  // Plate top Y = bottomY + thickness; all lengths use explicit Geant4 units.
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
  // All shielding sections share this outer radius; each entry has its own bore,
  // full axial height and World Y center. Inner radius zero means a solid disk.
  G4double shieldOuterRadius = 160.*mm;
  const std::array<ShieldPart, 3> innerShield{{
      {"Upper", 15.*mm, 20.*mm, -310.*mm},
      {"Body", 60.*mm, 100.*mm, -370.*mm},
      {"Lower", 0.*mm, 20.*mm, -430.*mm}
  }};
};
}

