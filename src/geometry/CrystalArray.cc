#include "geometry/CrystalArray.hh"
#include "geometry/GeometryParameters.hh"
#include "materials/Materials.hh"
#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4VisAttributes.hh"
#include "G4ios.hh"

namespace cupid {
void CrystalArray::Build(G4LogicalVolume* mother, const Materials& materials,
                        const GeometryParameters& p) const {
  const auto half = p.crystalSide/2.;
  auto* solid = new G4Box("CrystalArray.LMO.Solid", half, half, half);
  auto* logical = new G4LogicalVolume(solid, materials.Get("Li2MoO4"),
                                    "CrystalArray.LMO.Logical");
  logical->SetVisAttributes(G4VisAttributes(G4Colour(0.1, 0.8, 0.95)));
  for (G4int id = 0; id < p.crystalCount; ++id) {
    const auto offset = (id - (p.crystalCount-1)/2.) * (p.crystalSide+p.crystalGap);
    const auto position = p.crystalCenter + G4ThreeVector(offset, 0., 0.);
    // Copy numbers are crystal IDs within this single array in v0.1.
    new G4PVPlacement(nullptr, position, logical, "CrystalArray.LMO", mother, false, id);
    G4cout << "CUPID_CRYSTAL id=" << id << " position_mm=" << position/mm
           << " side_mm=" << p.crystalSide/mm << G4endl;
  }
}
}
