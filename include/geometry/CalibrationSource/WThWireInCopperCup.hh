/// \file include/geometry/CalibrationSource/WThWireInCopperCup.hh
/// \brief Build the WThWireInCopperCup calibration assembly beside LMO0.
///
/// Select WThWireInCopperCup with /cupid/geometry/enable before initialization.
/// LMOArray must also be enabled. The source follows LMO0's local -Z face;
/// dimensions and the 7.5 mm cup-face gap are configured in WThWireInCopperCup.cc.
/// This component builds passive materials only; a generator must emit decays.
#pragma once
#include "geometry/VGeometry.hh"

namespace cupid {

class WThWireInCopperCup : public VGeometry {
 public:
  void Build(G4VPhysicalVolume* parent, const Materials& materials,
             const GeometryParameters& parameters, GeometryManager& geometry) const override;
};
}  // namespace cupid
