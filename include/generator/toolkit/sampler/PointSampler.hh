/// \file include/generator/toolkit/sampler/PointSampler.hh
/// \brief Return a user-specified fixed position without geometry constraints.
///
/// Construct with finite coordinates in Geant4 length units, then call Sample()
/// from a concrete generator. For primary vertices, supply World coordinates.
/// No initialized geometry is required; geometry rebuilds do not affect the point.

#pragma once

#include "G4ThreeVector.hh"

namespace cupid {
class PointSampler {
 public:
  explicit PointSampler(const G4ThreeVector& point);
  // Return the configured position unchanged, without consuming random numbers.
  G4ThreeVector Sample() const;

 private:
  G4ThreeVector fPoint;
};
}
