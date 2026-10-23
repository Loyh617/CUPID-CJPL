#include "generator/common/VertexSampler.hh"
#include "geometry/GeometryManager.hh"
#include "G4LogicalVolume.hh"
#include "G4Navigator.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VSolid.hh"
#include "Randomize.hh"
#include <cmath>
#include <stdexcept>

namespace cupid {
VertexSampler::VertexSampler(const GeneratorConfig& config, const GeometryManager& geometry)
    : fGeometry(geometry), fGeometryGeneration(geometry.GetGeometryGeneration()),
      fConfig(config), fNavigator(std::make_unique<G4Navigator>()) {
  auto* world = geometry.GetWorldVolume();
  fNavigator->SetWorldVolume(world);
  if (fConfig.positionMode == "point") {
    const auto& p = fConfig.point;
    if (!std::isfinite(p.x()) || !std::isfinite(p.y()) || !std::isfinite(p.z()) ||
        world->GetLogicalVolume()->GetSolid()->Inside(p) != kInside)
      throw std::runtime_error("Source point must be strictly inside World.");
    return;
  }
  if (fConfig.positionMode != "volume" || fConfig.volume.empty())
    throw std::runtime_error("Volume sampling requires a physical volume name.");

  const auto& volume = geometry.FindSourceVolume(fConfig.volume);
  fTarget = volume.physicalVolume;
  fRotation = volume.localToWorldRotation;
  fTranslation = volume.localToWorldTranslation;
  fSolid = fTarget->GetLogicalVolume()->GetSolid();
  fSolid->BoundingLimits(fMinimum, fMaximum);
  for (int axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(fMinimum[axis]) || !std::isfinite(fMaximum[axis]) ||
        fMaximum[axis] <= fMinimum[axis])
      throw std::runtime_error("Source volume has invalid sampling bounds.");
  }
}
VertexSampler::~VertexSampler() = default;

G4ThreeVector VertexSampler::Sample() {
  if (fGeometry.GetGeometryGeneration() != fGeometryGeneration)
    throw std::runtime_error("Source sampler is stale after geometry reconstruction; recreate the generator.");
  if (fConfig.positionMode == "point") return fConfig.point;
  for (int attempt = 0; attempt < 100000; ++attempt) {
    G4ThreeVector local;
    for (int axis = 0; axis < 3; ++axis)
      local[axis] = fMinimum[axis] + G4UniformRand() * (fMaximum[axis] - fMinimum[axis]);
    if (fSolid->Inside(local) != kInside) continue;
    const auto global = fTranslation + fRotation * local;
    // A private navigator excludes daughters without changing tracking state.
    if (fNavigator->LocateGlobalPointAndSetup(global, nullptr, false) == fTarget)
      return global;
  }
  throw std::runtime_error("Source rejection sampling exhausted 100000 attempts in " + fConfig.volume);
}
}
