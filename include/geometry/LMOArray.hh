/// \file include/geometry/LMOArray.hh
/// \brief Declaration: Construct individually named LMO logical and physical volumes.
///
/// GeometryManager calls Build() when LMOArray is enabled. The builder uses
/// Li2MoO4, names volumes LMO0, LMO1, ... and assigns matching copy numbers.
/// It registers source placements and scoring logical volumes; sensitive
/// detectors are attached later by DetectorConstruction.

#pragma once
#include "geometry/VGeometry.hh"
namespace cupid {
class LMOArray : public VGeometry {
 public:
  void Build(G4VPhysicalVolume* parent, const Materials& materials,
             const GeometryParameters& parameters, GeometryManager& geometry) const override;
};
}

