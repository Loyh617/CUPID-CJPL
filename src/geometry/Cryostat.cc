#include "geometry/Cryostat.hh"
#include "geometry/GeometryParameters.hh"
#include "geometry/GeometryManager.hh"
#include "materials/Materials.hh"
#include "G4GenericPolycone.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4SubtractionSolid.hh"
#include "G4Transform3D.hh"
#include "G4Tubs.hh"
#include "G4VisAttributes.hh"
#include <array>
#include <initializer_list>
#include <vector>

namespace cupid {
namespace {
// Radius/axial coordinate pairs in mm describe measured engineering profiles.
// Solids stay in local coordinates; physical placements rotate their Z axis to Y.
using Point = std::array<G4double, 2>;
struct VesselSection {
  const char* name;
  const char* material;
  G4double originY;
  std::vector<Point> contour;
};
const std::vector<VesselSection>& Sections() {
  static const std::vector<VesselSection> sections{
    {"InnerUpper", "G4_Cu", 6.,
      {{264.5,0.},{270.,0.},{270.,12.},{265.,12.},{265.,248.},{270.,248.},{270.,260.},{264.5,260.}}},
    {"InnerLower", "G4_Cu", -619.5,
      {{0.,0.},{270.,0.},{270.,13.5},{260.5,13.5},{260.5,601.5},{270.,601.5},
       {270.,625.5},{260.,625.5},{260.,1.5},{0.,1.5}}},
    {"MiddleUpper", "G4_Cu", 15.,
      {{286.5,0.},{295.,0.},{295.,10.},{288.,10.},{288.,490.},{295.,490.},{295.,500.},{286.5,500.}}},
    {"MiddleLower", "G4_Cu", -634.5,
      {{0.,0.},{287.5,0.},{287.5,3.},{289.,3.},{289.,13.},{276.5,13.},
       {276.5,632.},{295.,632.},{295.,649.5},{275.5,649.5},{275.5,3.},{0.,3.}}},
    {"OuterUpper", "G4_Cu", 515.,
      {{321.5,0.},{330.,0.},{330.,10.},{323.,10.},{323.,277.},{330.,277.},{330.,287.},{321.5,287.}}},
    {"OuterMiddle", "G4_Cu", 15.,
      {{321.5,0.},{330.,0.},{330.,10.},{323.,10.},{323.,490.},{330.,490.},{330.,500.},{321.5,500.}}},
    {"OuterLower", "G4_Cu", -661.5,
      {{0.,0.},{310.,0.},{310.,3.},{311.5,3.},{311.5,659.},{330.,659.},
       {330.,676.5},{310.5,676.5},{310.5,13.},{298.,13.},{298.,3.},{0.,3.}}},
    {"AlUpper", "G4_Al", 513.7,
      {{340.,0.},{344.2,0.},{344.2,450.},{380.,450.},{380.,470.},{340.,470.}}},
    {"AlMiddle", "G4_Al", 103.7,
      {{340.,0.},{344.2,0.},{344.2,410.},{340.,410.}}},
    // Coarsened meridian of the curved bottom: a documented v0.1 approximation.
    // Retains the raised central floor, outer low point and cylindrical wall.
    {"AlLower", "G4_Al", -746.3,
      {{0.,76.},{118.231,76.},{148.149,73.328},{177.067,65.414},{204.021,52.574},
       {209.799,49.006},{264.911,13.42},{282.527,6.566},{292.005,5.925},
       {310.383,10.343},{325.539,21.639},{335.023,37.99},{337.451,52.},
       {340.,52.},{340.,850.},{344.2,850.},{344.2,52.},{343.822,52.},
       {341.244,37.426},{334.809,24.098},{321.101,9.971},{303.242,1.692},
       {288.537,0.01525879},{273.929,2.391},{260.513,8.641},{206.588,43.461},
       {183.021,56.059},{155.133,65.232},{125.952,69.233},{118.6,69.4},{0.,69.4}}}
  };
  return sections;
}
}

void Cryostat::Build(G4VPhysicalVolume* parent, const Materials& materials,
                     const GeometryParameters& parameters, GeometryManager& geometry) const {
  auto* mother = parent->GetLogicalVolume();
  G4RotationMatrix rotation;
  rotation.rotateX(-90.*deg);
  G4int copy = 0;
  auto place = [&](const G4String& name, G4VSolid* solid, const char* material,
                   G4double y, G4double alpha) {
    auto* logical = new G4LogicalVolume(solid, materials.Get(material), name+".Logical");
    const bool copper = G4String(material) == "G4_Cu";
    G4VisAttributes attributes(copper ? G4Colour(0.75,0.45,0.2,alpha)
                                    : G4Colour(0.7,0.8,0.9,alpha));
    attributes.SetForceSolid(true);
    attributes.SetForceLineSegmentsPerCircle(32);
    logical->SetVisAttributes(attributes);
    auto* physical = new G4PVPlacement(G4Transform3D(rotation, {0.,y,0.}), logical, name, mother, false, copy++);
    geometry.RegisterSourceVolume(physical, parent);
  };

  for (const auto& plate : parameters.plates) {
    const G4String name = G4String("Cryostat.Plate.") + plate.name;
    G4VSolid* solid = new G4Tubs(name+".Disk", 0., plate.radius,
                                plate.thickness/2., 0., 360.*deg);
    if (plate.serviceHoles) {
      auto* hole = new G4Tubs(name+".Bore", 0., 8.5*mm, plate.thickness/2.+mm, 0., 360.*deg);
      // World X,Z locations converted to local X,Y by the placement rotation.
      const std::array<Point,3> holes{{{27.5,-15.8771},{-27.5,-15.8771},{0.,31.7543}}};
      for (std::size_t i=0; i<holes.size(); ++i) {
        solid = new G4SubtractionSolid(name+".BoreCut"+std::to_string(i), solid, hole,
                    nullptr, {holes[i][0]*mm, holes[i][1]*mm, 0.});
      }
    }
    place(name, solid, plate.material, plate.bottomY+plate.thickness/2., 1.);
  }

  for (const auto& section : Sections()) {
    const G4String name = G4String("Cryostat.Vessel.") + section.name;
    std::vector<G4double> radius, height;
    for (const auto& point : section.contour) {
      radius.push_back(point[0]*mm);
      height.push_back(point[1]*mm);
    }
    auto* solid = new G4GenericPolycone(name+".Solid", 0., 360.*deg,
                static_cast<G4int>(radius.size()), radius.data(), height.data());
    place(name, solid, section.material, section.originY*mm, 0.15);
  }
}
}

