#include "DetectorConstruction.hh"
#include "geometry/GeometryManager.hh"
#include "materials/Materials.hh"
#include "LMOSensitiveDetector.hh"

#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4SDManager.hh"
#include "G4VisAttributes.hh"

#include <stdexcept>

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
  fGeometry->Build(world, materials);
  return world;
}

void DetectorConstruction::ConstructSDandField() {
  const auto& lmos = fGeometry->GetLMOLogicalVolumes();
  if (lmos.empty()) return;

  // G4SDManager and the detector instance belong to the calling thread.
  auto* manager = G4SDManager::GetSDMpointer();
  auto* existing = manager->FindSensitiveDetector(LMOSensitiveDetector::DetectorName, false);
  auto* detector = dynamic_cast<LMOSensitiveDetector*>(existing);
  if (existing && !detector)
    throw std::runtime_error("LMOSD is already registered with a different detector type.");
  if (!detector) {
    detector = new LMOSensitiveDetector;
    manager->AddNewDetector(detector);
  }
  for (auto* logical : lmos) {
    if (logical->GetSensitiveDetector() != detector)
      SetSensitiveDetector(logical, detector);
  }
}
}
