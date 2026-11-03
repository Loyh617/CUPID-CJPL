/// \file src/generator/toolkit/sampler/PointSampler.cc
/// \brief Store and return a fixed position independently of detector geometry.
///
/// Sample() imposes no volume or World-boundary restriction. The generator is
/// responsible for choosing a position suitable for its particle-source setup.

#include "generator/toolkit/sampler/PointSampler.hh"

#include <cmath>
#include <stdexcept>

namespace cupid {
PointSampler::PointSampler(const G4ThreeVector& point) : fPoint(point) {
  if (!std::isfinite(point.x()) || !std::isfinite(point.y()) || !std::isfinite(point.z()))
    throw std::runtime_error("Source point coordinates must be finite.");
}

G4ThreeVector PointSampler::Sample() const {
  return fPoint;
}
}
