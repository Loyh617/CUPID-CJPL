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
