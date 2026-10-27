#include "geometry/GeometryManager.hh"
#include "materials/Materials.hh"

#include "G4Box.hh"
#include "G4GeometryManager.hh"
#include "G4LogicalVolume.hh"
#include "G4Navigator.hh"
#include "G4PVPlacement.hh"
#include "G4SystemOfUnits.hh"

#include <cmath>
#include <iostream>
#include <stdexcept>

int main() {
  try {
    cupid::Materials materials;
    materials.Build();
    cupid::GeometryManager geometry;
    for (const char* name : {"Cryostat", "InnerShielding", "LMOArray"}) geometry.SetEnabled(name, true);
    geometry.SetLMOCount(2);
    auto* logical = new G4LogicalVolume(new G4Box("World.Solid", 2*m, 2*m, 2*m),
                                       materials.Get("G4_Galactic"), "World.Logical");
    auto* world = new G4PVPlacement(nullptr, {}, logical, "World", nullptr, false, 0);
    geometry.Build(world, materials);
    G4GeometryManager::GetInstance()->CloseGeometry();
    const G4ThreeVector source(-354.2*mm, -350*mm, 0.);
    const G4ThreeVector direction(1., 0., 0.);
    const auto& lmo = geometry.FindSourceVolume("LMO0");
    if ((lmo.localToWorldTranslation - G4ThreeVector(-25*mm, -350*mm, 0.)).mag() > 1.e-8*mm)
      throw std::runtime_error("Unexpected LMO0 placement.");
    if ((lmo.localToWorldTranslation - source).unit().dot(direction) < 1. - 1.e-12)
      throw std::runtime_error("Source is not facing LMO0.");
    const auto& vessel = geometry.FindSourceVolume("Cryostat.Vessel.AlLower");
    const auto inverse = vessel.localToWorldRotation.inverse();
    auto* solid = vessel.physicalVolume->GetLogicalVolume()->GetSolid();
    const auto localPoint = inverse * (source - vessel.localToWorldTranslation);
    const auto distance = solid->DistanceToIn(localPoint, inverse * direction);
    G4Navigator navigator;
    navigator.SetWorldVolume(world);
    if (navigator.LocateGlobalPointAndSetup(source, nullptr, false) != world)
      throw std::runtime_error("Source is not in the outside vacuum.");
    G4double safety = 0.;
    const auto step = navigator.ComputeStep(source, direction, 1*m, safety);
    if (std::abs(distance - 10*mm) > 1.e-8*mm || std::abs(step - 10*mm) > 1.e-8*mm)
      throw std::runtime_error("Source is not 10 mm from the first cryostat surface.");
    std::cout << "TH232_SOURCE position_mm=" << source/mm << " first_surface_distance_mm="
              << step/mm << " target=LMO0 result=PASS\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
