/// \file src/generator/toolkit/sampler/DirectVolumeSampler.cc
/// \brief Implementation: Sample uniformly by volume in analytic boxes, tubes and spheres.
///
/// A concrete generator supplies a center and local-to-World rotation, then
/// calls SampleBox(), SampleTubs() or SampleSphere() in Geant4 units.
/// Static overloads read a registered placement and reuse the same formulas.
/// Box dimensions and tube height use half lengths; radii are not diameters.
/// Points are in World coordinates. Geometry overloads reject daughter volumes.

#include "generator/toolkit/sampler/DirectVolumeSampler.hh"
#include "geometry/GeometryManager.hh"

#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4Orb.hh"
#include "G4RandomDirection.hh"
#include "G4Sphere.hh"
#include "G4Tubs.hh"
#include "G4VPhysicalVolume.hh"
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

void ValidateRadii(G4double innerRadius, G4double outerRadius) {
  RequirePositiveLength(outerRadius);
  if (!std::isfinite(innerRadius) || innerRadius < 0. || innerRadius >= outerRadius)
    throw std::invalid_argument("Sampler radii must satisfy 0 <= innerRadius < outerRadius.");
}

void ValidatePhi(G4double startPhi, G4double deltaPhi) {
  if (!std::isfinite(startPhi) || !std::isfinite(deltaPhi) ||
      deltaPhi <= 0. || deltaPhi > twopi)
    throw std::invalid_argument("Sampler phi requires a finite start and 0 < deltaPhi <= 2*pi.");
}

void ValidateTheta(G4double startTheta, G4double deltaTheta) {
  if (!std::isfinite(startTheta) || !std::isfinite(deltaTheta) ||
      startTheta < 0. || startTheta >= pi ||
      deltaTheta <= 0. || deltaTheta > pi - startTheta)
    throw std::invalid_argument(
        "Sampler theta requires 0 <= startTheta < pi and 0 < deltaTheta <= pi - startTheta.");
}

const SourceVolumeInfo& FindLeafVolume(
    const G4String& volume, const GeometryManager& geometry) {
  // Lookup also requires completed geometry registration. No World traversal is needed.
  const auto& info = geometry.FindSourceVolume(volume);
  if (info.physicalVolume->GetLogicalVolume()->GetNoDaughters() != 0)
    throw std::invalid_argument(
        "Direct volume sampling requires a placement without daughters: " + volume +
        ". Use RejectionVolumeSampler to exclude daughters.");
  return info;
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

G4ThreeVector DirectVolumeSampler::SampleTubs(
    G4double innerRadius, G4double outerRadius, G4double halfLength,
    G4double startPhi, G4double deltaPhi) const {
  ValidateRadii(innerRadius, outerRadius);
  RequirePositiveLength(halfLength);
  ValidatePhi(startPhi, deltaPhi);
  // Uniform volume requires uniform r^2 between the squared radii.
  // Work with a dimensionless ratio to avoid squaring dimensional lengths.
  const auto ratio = innerRadius / outerRadius;
  const auto ratioSquared = ratio * ratio;
  const auto r = outerRadius * std::sqrt(
      ratioSquared + (1. - ratioSquared) * G4UniformRand());
  const auto phi = std::remainder(startPhi, twopi) + deltaPhi * G4UniformRand();
  const auto z = (2.*G4UniformRand() - 1.) * halfLength;
  return ToWorld({r * std::cos(phi), r * std::sin(phi), z});
}

G4ThreeVector DirectVolumeSampler::SampleSphere(
    G4double innerRadius, G4double outerRadius,
    G4double startPhi, G4double deltaPhi,
    G4double startTheta, G4double deltaTheta) const {
  ValidateRadii(innerRadius, outerRadius);
  ValidatePhi(startPhi, deltaPhi);
  ValidateTheta(startTheta, deltaTheta);
  // Uniform volume requires uniform r^3 between the cubed radii.
  const auto ratio = innerRadius / outerRadius;
  const auto ratioCubed = ratio * ratio * ratio;
  const auto r = outerRadius * std::cbrt(
      ratioCubed + (1. - ratioCubed) * G4UniformRand());
  // Reuse Geant4 for full solid angle. This direction locates a position;
  // it does not set the emitted particle momentum.
  if (deltaPhi == twopi && startTheta == 0. && deltaTheta == pi)
    return ToWorld(r * G4RandomDirection());

  // Uniform solid angle requires uniform cos(theta), not uniform theta.
  const auto cosMinimum = std::cos(startTheta + deltaTheta);
  const auto cosMaximum = std::cos(startTheta);
  const auto cosTheta = cosMinimum + (cosMaximum - cosMinimum) * G4UniformRand();
  const auto sinTheta = std::sqrt(std::max(0., (1. - cosTheta) * (1. + cosTheta)));
  const auto phi = std::remainder(startPhi, twopi) + deltaPhi * G4UniformRand();
  return ToWorld({r * sinTheta * std::cos(phi), r * sinTheta * std::sin(phi), r * cosTheta});
}

G4ThreeVector DirectVolumeSampler::SampleBox(
    const G4String& volume, const GeometryManager& geometry) {
  const auto& info = FindLeafVolume(volume, geometry);
  const auto* solid = dynamic_cast<const G4Box*>(
      info.physicalVolume->GetLogicalVolume()->GetSolid());
  if (!solid) throw std::invalid_argument("SampleBox requires G4Box: " + volume);
  const DirectVolumeSampler sampler(info.localToWorldTranslation, info.localToWorldRotation);
  return sampler.SampleBox({solid->GetXHalfLength(), solid->GetYHalfLength(), solid->GetZHalfLength()});
}

G4ThreeVector DirectVolumeSampler::SampleTubs(
    const G4String& volume, const GeometryManager& geometry) {
  const auto& info = FindLeafVolume(volume, geometry);
  const auto* solid = dynamic_cast<const G4Tubs*>(
      info.physicalVolume->GetLogicalVolume()->GetSolid());
  if (!solid) throw std::invalid_argument("SampleTubs requires G4Tubs: " + volume);
  const DirectVolumeSampler sampler(info.localToWorldTranslation, info.localToWorldRotation);
  return sampler.SampleTubs(solid->GetInnerRadius(), solid->GetOuterRadius(),
      solid->GetZHalfLength(), solid->GetStartPhiAngle(), solid->GetDeltaPhiAngle());
}

G4ThreeVector DirectVolumeSampler::SampleSphere(
    const G4String& volume, const GeometryManager& geometry) {
  const auto& info = FindLeafVolume(volume, geometry);
  const auto* solid = info.physicalVolume->GetLogicalVolume()->GetSolid();
  const DirectVolumeSampler sampler(info.localToWorldTranslation, info.localToWorldRotation);
  if (const auto* sphere = dynamic_cast<const G4Sphere*>(solid)) {
    return sampler.SampleSphere(sphere->GetInnerRadius(), sphere->GetOuterRadius(),
        sphere->GetStartPhiAngle(), sphere->GetDeltaPhiAngle(),
        sphere->GetStartThetaAngle(), sphere->GetDeltaThetaAngle());
  }
  if (const auto* orb = dynamic_cast<const G4Orb*>(solid))
    return sampler.SampleSphere(0., orb->GetRadius());
  throw std::invalid_argument("SampleSphere requires G4Sphere or G4Orb: " + volume);
}
}
