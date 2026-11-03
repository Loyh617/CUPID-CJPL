/// \file src/generator/toolkit/sampler/RejectionVolumeSampler.cc
/// \brief Implementation: Sample uniformly inside one registered physical placement by rejection.
///
/// Construct with a volume name and an initialized GeometryManager, then
/// call Sample() for World coordinates. Daughter volumes are excluded.
/// Keep a sampler per worker and recreate it after geometry reconstruction;
/// sampling fails explicitly if the bounded attempt limit is exhausted.

#include "generator/toolkit/sampler/RejectionVolumeSampler.hh"
#include "geometry/GeometryManager.hh"

#include "G4LogicalVolume.hh"
#include "G4Navigator.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VSolid.hh"
#include "Randomize.hh"

#include <cmath>
#include <stdexcept>

namespace cupid {
RejectionVolumeSampler::RejectionVolumeSampler(
    const G4String& volume, const GeometryManager& geometry)
    : fGeometry(geometry), fGeometryGeneration(geometry.GetGeometryGeneration()),
      fVolume(volume), fNavigator(std::make_unique<G4Navigator>()) {
  auto* world = geometry.GetWorldVolume();
  fNavigator->SetWorldVolume(world);
  if (volume.empty())
    throw std::runtime_error("Volume sampling requires a physical volume name.");
  const auto& info = geometry.FindSourceVolume(volume);
  fTarget = info.physicalVolume;
  fRotation = info.localToWorldRotation;
  fTranslation = info.localToWorldTranslation;
  fSolid = fTarget->GetLogicalVolume()->GetSolid();
  fSolid->BoundingLimits(fMinimum, fMaximum);
  for (int axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(fMinimum[axis]) || !std::isfinite(fMaximum[axis]) ||
        fMaximum[axis] <= fMinimum[axis])
      throw std::runtime_error("Source volume has invalid sampling bounds.");
  }
}
RejectionVolumeSampler::~RejectionVolumeSampler() = default;

G4ThreeVector RejectionVolumeSampler::Sample() {
  if (fGeometry.GetGeometryGeneration() != fGeometryGeneration)
    throw std::runtime_error("Source sampler is stale after geometry reconstruction; recreate the generator.");
  // Safety limit per sampled point, not a physical cut. Very small acceptance
  // calls for a better proposal region; do not silently return a biased fallback.
  for (int attempt = 0; attempt < 100000; ++attempt) {
    G4ThreeVector local;
    for (int axis = 0; axis < 3; ++axis)
      local[axis] = fMinimum[axis] + G4UniformRand() * (fMaximum[axis] - fMinimum[axis]);
    // The local bounding box is only a proposal; accept the actual solid interior.
    if (fSolid->Inside(local) != kInside) continue;
    const auto global = fTranslation + fRotation * local;
    // A private navigator excludes daughters without changing tracking state.
    if (fNavigator->LocateGlobalPointAndSetup(global, nullptr, false) == fTarget)
      return global;
  }
  throw std::runtime_error("Source rejection sampling exhausted 100000 attempts in " + fVolume);
}
}
