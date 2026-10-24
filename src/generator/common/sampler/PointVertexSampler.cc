#include "generator/common/sampler/PointVertexSampler.hh"
#include "geometry/GeometryManager.hh"

#include "G4LogicalVolume.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VSolid.hh"

#include <cmath>
#include <stdexcept>

namespace cupid {
PointVertexSampler::PointVertexSampler(
    const G4ThreeVector& point, const GeometryManager& geometry)
    : fGeometry(geometry), fGeometryGeneration(geometry.GetGeometryGeneration()),
      fPoint(point) {
  auto* world = geometry.GetWorldVolume();
  if (!std::isfinite(point.x()) || !std::isfinite(point.y()) || !std::isfinite(point.z()) ||
      world->GetLogicalVolume()->GetSolid()->Inside(point) != kInside)
    throw std::runtime_error("Source point must be strictly inside World.");
}

G4ThreeVector PointVertexSampler::Sample() {
  if (fGeometry.GetGeometryGeneration() != fGeometryGeneration)
    throw std::runtime_error("Source sampler is stale after geometry reconstruction; recreate the generator.");
  return fPoint;
}
}
