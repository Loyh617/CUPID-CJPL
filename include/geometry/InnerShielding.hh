/// \file include/geometry/InnerShielding.hh
/// \brief Declaration: Construct one copper inner shield with a stepped axial bore.
///
/// GeometryManager calls Build(parent, materials, parameters, geometry) when
/// InnerShielding is enabled. Three dimension sections define a single
/// G4Polycone; the sole source placement is registered as InnerShielding.

#pragma once
#include "geometry/VGeometry.hh"
namespace cupid {
class InnerShielding : public VGeometry {
 public:
  void Build(G4VPhysicalVolume* parent, const Materials& materials,
             const GeometryParameters& parameters, GeometryManager& geometry) const override;
};
}

