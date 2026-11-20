/// \file src/geometry/InnerShielding.cc
/// \brief Construct one copper inner shield with a stepped axial bore.
///
/// GeometryManager calls Build() when InnerShielding is enabled. The three
/// dimension sections define one G4Polycone, one logical volume and one
/// registered physical placement named InnerShielding.

#include "geometry/InnerShielding.hh"
#include "geometry/GeometryParameters.hh"
#include "geometry/GeometryManager.hh"
#include "materials/Materials.hh"

#include "G4GeometryTolerance.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4Polycone.hh"
#include "G4Transform3D.hh"
#include "G4VisAttributes.hh"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace cupid {
void InnerShielding::Build(G4VPhysicalVolume* parent, const Materials& materials,
                          const GeometryParameters& p, GeometryManager& geometry) const {
  // Dimensions remain in GeometryParameters; sections must meet without gaps or overlaps.
  auto sections = p.innerShield;
  if (!std::isfinite(p.shieldOuterRadius) || p.shieldOuterRadius <= 0.)
    throw std::invalid_argument("InnerShielding requires a positive finite outer radius.");
  for (const auto& part : sections) {
    if (!std::isfinite(part.centerY) || !std::isfinite(part.height) || part.height <= 0. ||
        !std::isfinite(part.innerRadius) || part.innerRadius < 0. ||
        part.innerRadius >= p.shieldOuterRadius)
      throw std::invalid_argument("Invalid InnerShielding section dimensions.");
  }
  std::sort(sections.begin(), sections.end(), [](const auto& a, const auto& b) {
    return a.centerY < b.centerY;
  });
  const auto bottom = sections.front().centerY - sections.front().height/2.;
  const auto top = sections.back().centerY + sections.back().height/2.;
  const auto centerY = (bottom + top)/2.;
  const auto tolerance = G4GeometryTolerance::GetInstance()->GetSurfaceTolerance();
  std::vector<G4double> z, inner, outer;
  auto previousTop = bottom;
  for (const auto& part : sections) {
    const auto partBottom = part.centerY - part.height/2.;
    const auto partTop = part.centerY + part.height/2.;
    if (std::abs(partBottom - previousTop) > tolerance)
      throw std::invalid_argument("InnerShielding sections must have touching axial faces.");
    // Repeated axial planes retain sharp bore steps instead of interpolated cones.
    // Coincident faces are shared within Geant4's surface tolerance.
    z.push_back(previousTop - centerY);
    z.push_back(partTop - centerY);
    inner.push_back(part.innerRadius);
    inner.push_back(part.innerRadius);
    outer.push_back(p.shieldOuterRadius);
    outer.push_back(p.shieldOuterRadius);
    previousTop = partTop;
  }
  auto* solid = new G4Polycone("InnerShielding", 0., 360.*deg,
      static_cast<G4int>(z.size()), z.data(), inner.data(), outer.data());
  // Shield material is selected here; all axial sections share the same material.
  auto* logical = new G4LogicalVolume(solid, materials.Get("G4_Cu"), "InnerShielding");
  logical->SetVisAttributes(G4VisAttributes(G4Colour(0.85, 0.5, 0.2, 0.35)));
  G4RotationMatrix rotation;
  rotation.rotateX(-90.*deg);  // Map the solid's local +Z axis onto the assembly +Y axis.
  auto* physical = new G4PVPlacement(G4Transform3D(rotation, {0., centerY, 0.}), logical,
      "InnerShielding", parent->GetLogicalVolume(), false, 0);
  geometry.RegisterSourceVolume(physical, parent);
}
}
