#include "geometry/InnerShielding.hh"
#include "geometry/GeometryParameters.hh"
#include "materials/Materials.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4Tubs.hh"
#include "G4Transform3D.hh"
#include "G4VisAttributes.hh"

namespace cupid {
void InnerShielding::Build(G4LogicalVolume* mother, const Materials& materials,
                          const GeometryParameters& p) const {
  G4RotationMatrix rotation;
  rotation.rotateX(-90.*deg);
  G4int copy = 0;
  for (const auto& part : p.innerShield) {
    const G4String name = G4String("InnerShielding.") + part.name;
    auto* solid = new G4Tubs(name+".Solid", part.innerRadius, p.shieldOuterRadius,
                            part.height/2., 0., 360.*deg);
    auto* logical = new G4LogicalVolume(solid, materials.Get("G4_Cu"), name+".Logical");
    logical->SetVisAttributes(G4VisAttributes(G4Colour(0.85, 0.5, 0.2, 0.35)));
    new G4PVPlacement(G4Transform3D(rotation, {0., part.centerY, 0.}), logical,
                      name, mother, false, copy++);
  }
}
}

