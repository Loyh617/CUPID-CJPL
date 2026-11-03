/// \file src/generator/toolkit/sampler/DirectVolumeSampler.cc
/// \brief Implementation: Sample uniformly by volume in analytic boxes, cylinders and spheres.
///
/// A concrete generator supplies a center and local-to-World rotation, then
/// calls SampleBox(), SampleCylinder() or SampleSphere() in Geant4 units.
/// Box dimensions and cylinder height use half lengths. Returned points are
/// in World coordinates; no navigation or daughter-volume exclusion occurs.

#include "generator/toolkit/sampler/DirectVolumeSampler.hh"

#include "G4RandomDirection.hh"
#include "G4PhysicalConstants.hh"
#include "Randomize.hh"

#include <cmath>
#include <stdexcept>

namespace cupid {
namespace {
void RequirePositiveLength(G4double value) {
  if (!std::isfinite(value) || value <= 0.)
    throw std::invalid_argument("Sampler dimensions must be finite and strictly positive.");
}

void ValidateFrame(const G4ThreeVector& center, const G4RotationMatrix& rotation) {
  for (int axis = 0; axis < 3; ++axis)
    if (!std::isfinite(center[axis]))
      throw std::invalid_argument("Sampler center must have finite coordinates.");
  const G4ThreeVector axes[] = {rotation * G4ThreeVector(1., 0., 0.),
                                rotation * G4ThreeVector(0., 1., 0.),
                                rotation * G4ThreeVector(0., 0., 1.)};
  for (const auto& axis : axes)
    for (int component = 0; component < 3; ++component)
      if (!std::isfinite(axis[component]))
        throw std::invalid_argument("Sampler rotation must be finite.");
  constexpr G4double tolerance = 1.e-10;
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j)
      if (std::abs(axes[i].dot(axes[j]) - (i == j ? 1. : 0.)) > tolerance)
        throw std::invalid_argument("Sampler rotation must be orthonormal.");
  if (std::abs(axes[0].cross(axes[1]).dot(axes[2]) - 1.) > tolerance)
    throw std::invalid_argument("Sampler rotation must preserve handedness.");
}
}

DirectVolumeSampler::DirectVolumeSampler(
    const G4ThreeVector& center, const G4RotationMatrix& localToWorldRotation)
    : fCenter(center), fRotation(localToWorldRotation) {
  ValidateFrame(center, localToWorldRotation);
}

G4ThreeVector DirectVolumeSampler::ToWorld(const G4ThreeVector& local) const {
  const auto world = fCenter + fRotation * local;
  for (int axis = 0; axis < 3; ++axis)
    if (!std::isfinite(world[axis]))
      throw std::overflow_error("Sampled world position is not finite.");
  return world;
}

G4ThreeVector DirectVolumeSampler::SampleBox(const G4ThreeVector& halfLengths) const {
  for (int axis = 0; axis < 3; ++axis) RequirePositiveLength(halfLengths[axis]);
  G4ThreeVector local;
  for (int axis = 0; axis < 3; ++axis)
    local[axis] = (2.*G4UniformRand() - 1.) * halfLengths[axis];
  return ToWorld(local);
}

G4ThreeVector DirectVolumeSampler::SampleCylinder(G4double radius, G4double halfLength) const {
  RequirePositiveLength(radius);
  RequirePositiveLength(halfLength);
  // The radial CDF is r^2 / R^2 for a uniform disk.
  const auto r = radius * std::sqrt(G4UniformRand());
  const auto phi = twopi * G4UniformRand();
  const auto z = (2.*G4UniformRand() - 1.) * halfLength;
  return ToWorld({r * std::cos(phi), r * std::sin(phi), z});
}

G4ThreeVector DirectVolumeSampler::SampleSphere(G4double radius) const {
  RequirePositiveLength(radius);
  // Uniform volume needs r^3/R^3 uniform; the random direction locates the point,
  // and does not set the emitted particle momentum.
  const auto r = radius * std::cbrt(G4UniformRand());
  return ToWorld(r * G4RandomDirection());
}
}
