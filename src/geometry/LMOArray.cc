#include "geometry/LMOArray.hh"
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
void LMOArray::Build(G4VPhysicalVolume* parent, const Materials& materials,
                        const GeometryParameters& p, GeometryManager& geometry) const {
  auto* mother = parent->GetLogicalVolume();
  const auto half = p.lmoSide/2.;
  auto* solid = new G4Box("LMOArray.Solid", half, half, half);
  for (G4int id = 0; id < p.lmoCount; ++id) {
    // Match both volume names to the zero-based crystal copy number.
    const G4String lmoName = "LMO" + std::to_string(id);
    auto* logical = new G4LogicalVolume(solid, materials.Get("Li2MoO4"), lmoName);
    logical->SetVisAttributes(G4VisAttributes(G4Colour(0.1, 0.8, 0.95)));
    const auto offset = (id - (p.lmoCount-1)/2.) * (p.lmoSide+p.lmoGap);
    const auto position = p.lmoCenter + G4ThreeVector(offset, 0., 0.);
    // Copy numbers are crystal IDs within this single array in v0.1.
    auto* physical = new G4PVPlacement(nullptr, position, logical, lmoName, mother, false, id);
    geometry.RegisterSourceVolume(physical, parent);
    geometry.RegisterLMOVolume(physical);
    G4cout << "CUPID_LMO id=" << id << " position_mm=" << position/mm
           << " side_mm=" << p.lmoSide/mm << G4endl;
  }
}
}
