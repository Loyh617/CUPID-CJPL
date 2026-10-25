#include "generator/toolkit/sampler/DirectSurfaceSampler.hh"

#include "G4PhysicalConstants.hh"
#include "Randomize.hh"

#include <algorithm>
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

DirectSurfaceSampler::DirectSurfaceSampler(
    const G4ThreeVector& center, const G4RotationMatrix& localToWorldRotation)
    : fCenter(center), fRotation(localToWorldRotation) {
  ValidateFrame(center, localToWorldRotation);
}

G4ThreeVector DirectSurfaceSampler::ToWorld(const G4ThreeVector& local) const {
  const auto world = fCenter + fRotation * local;
  for (int axis = 0; axis < 3; ++axis)
    if (!std::isfinite(world[axis]))
      throw std::overflow_error("Sampled world position is not finite.");
  return world;
}

G4ThreeVector DirectSurfaceSampler::SampleRectangle(G4double halfX, G4double halfY) const {
  RequirePositiveLength(halfX);
  RequirePositiveLength(halfY);
  const auto x = (2.*G4UniformRand() - 1.) * halfX;
  const auto y = (2.*G4UniformRand() - 1.) * halfY;
  return ToWorld({x, y, 0.});
}

G4ThreeVector DirectSurfaceSampler::SampleSphere(G4double radius) const {
  RequirePositiveLength(radius);
  // Surface area is uniform in cos(theta), phi, not in theta, phi.
  const auto cosTheta = 2.*G4UniformRand() - 1.;
  const auto phi = twopi * G4UniformRand();
  const auto transverse = radius * std::sqrt(std::max(0., 1. - cosTheta*cosTheta));
  return ToWorld({transverse * std::cos(phi), transverse * std::sin(phi), radius * cosTheta});
}

G4ThreeVector DirectSurfaceSampler::SampleHemisphere(G4double radius) const {
  RequirePositiveLength(radius);
  const auto cosTheta = G4UniformRand();
  const auto phi = twopi * G4UniformRand();
  const auto transverse = radius * std::sqrt(std::max(0., 1. - cosTheta*cosTheta));
  return ToWorld({transverse * std::cos(phi), transverse * std::sin(phi), radius * cosTheta});
}
}
