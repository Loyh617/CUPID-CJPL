#include "DetectorConstruction.hh"
#include "geometry/GeometryManager.hh"
#include "materials/Materials.hh"
#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4VisAttributes.hh"

namespace cupid {
DetectorConstruction::DetectorConstruction()
    : fGeometry(std::make_unique<GeometryManager>()) {}
DetectorConstruction::~DetectorConstruction() = default;

G4VPhysicalVolume* DetectorConstruction::Construct() {
  Materials materials;
  materials.Build();
  const auto half = fGeometry->Parameters().worldHalfLength;
  auto* solid = new G4Box("World.Solid", half, half, half);
  auto* logical = new G4LogicalVolume(solid, materials.Get("G4_Galactic"), "World.Logical");
  logical->SetVisAttributes(G4VisAttributes::GetInvisible());
  auto* world = new G4PVPlacement(nullptr, {}, logical, "World", nullptr, false, 0);
  fGeometry->Build(logical, materials);
  return world;
}
}

