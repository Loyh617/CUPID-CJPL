#include "geometry/CrystalArray.hh"
#include "geometry/GeometryParameters.hh"
#include "geometry/GeometryManager.hh"
#include "materials/Materials.hh"
#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4VisAttributes.hh"
#include "G4ios.hh"
#include <string>

namespace cupid {
void CrystalArray::Build(G4VPhysicalVolume* parent, const Materials& materials,
                        const GeometryParameters& p, GeometryManager& geometry) const {
  auto* mother = parent->GetLogicalVolume();
  const auto half = p.crystalSide/2.;
  auto* solid = new G4Box("CrystalArray.LMO.Solid", half, half, half);
  for (G4int id = 0; id < p.crystalCount; ++id) {
    // Match both volume names to the zero-based crystal copy number.
    const G4String crystalName = "Crystal" + std::to_string(id);
    auto* logical = new G4LogicalVolume(solid, materials.Get("Li2MoO4"), crystalName);
    logical->SetVisAttributes(G4VisAttributes(G4Colour(0.1, 0.8, 0.95)));
    const auto offset = (id - (p.crystalCount-1)/2.) * (p.crystalSide+p.crystalGap);
    const auto position = p.crystalCenter + G4ThreeVector(offset, 0., 0.);
    // Copy numbers are crystal IDs within this single array in v0.1.
    auto* physical = new G4PVPlacement(nullptr, position, logical, crystalName, mother, false, id);
    geometry.RegisterSourceVolume(physical, parent);
    G4cout << "CUPID_CRYSTAL id=" << id << " position_mm=" << position/mm
           << " side_mm=" << p.crystalSide/mm << G4endl;
  }
}
}
