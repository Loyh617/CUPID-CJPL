/// \file src/generator/toolkit/sampler/PointSampler.cc
/// \brief Implementation: Return a fixed source position expressed in World coordinates.
///
/// Construct with a point and an initialized GeometryManager, then call
/// Sample() from a concrete generator. The point must be strictly inside
/// World; recreate the sampler after geometry reconstruction.

#include "generator/toolkit/sampler/PointSampler.hh"
#include "geometry/GeometryManager.hh"

#include "G4LogicalVolume.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VSolid.hh"

#include <cmath>
#include <stdexcept>

namespace cupid {
PointSampler::PointSampler(
    const G4ThreeVector& point, const GeometryManager& geometry)
    : fGeometry(geometry), fGeometryGeneration(geometry.GetGeometryGeneration()),
      fPoint(point) {
  auto* world = geometry.GetWorldVolume();
  if (!std::isfinite(point.x()) || !std::isfinite(point.y()) || !std::isfinite(point.z()) ||
      world->GetLogicalVolume()->GetSolid()->Inside(point) != kInside)
    throw std::runtime_error("Source point must be strictly inside World.");
}

G4ThreeVector PointSampler::Sample() {
  if (fGeometry.GetGeometryGeneration() != fGeometryGeneration)
    throw std::runtime_error("Source sampler is stale after geometry reconstruction; recreate the generator.");
  return fPoint;
}
}
