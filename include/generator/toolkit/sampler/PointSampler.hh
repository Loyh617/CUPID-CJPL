/// \file include/generator/toolkit/sampler/PointSampler.hh
/// \brief Declaration: Return a fixed source position expressed in World coordinates.
///
/// Construct with a point and an initialized GeometryManager, then call
/// Sample() from a concrete generator. The point must be strictly inside
/// World; recreate the sampler after geometry reconstruction.

#pragma once

#include "G4ThreeVector.hh"

#include <cstddef>

namespace cupid {
class GeometryManager;
// A fixed world-coordinate position, validated against the initialized World.
class PointSampler {
 public:
  PointSampler(const G4ThreeVector& point, const GeometryManager& geometry);
  G4ThreeVector Sample();

 private:
  const GeometryManager& fGeometry;
  std::size_t fGeometryGeneration;
  G4ThreeVector fPoint;
};
}
