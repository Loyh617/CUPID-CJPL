#include "generator/common/VertexSampler.hh"
#include "G4LogicalVolume.hh"
#include "G4Navigator.hh"
#include "G4TransportationManager.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VSolid.hh"
#include "Randomize.hh"
#include <cmath>
#include <functional>
#include <stdexcept>

namespace cupid {
VertexSampler::VertexSampler(const GeneratorConfig& config)
    : fConfig(config), fNavigator(std::make_unique<G4Navigator>()) {
  auto* world = G4TransportationManager::GetTransportationManager()
                    ->GetNavigatorForTracking()->GetWorldVolume();
  if (!world) throw std::runtime_error("Source sampling requires initialized geometry.");
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

  G4int matches = 0;
  std::function<void(G4VPhysicalVolume*, const G4RotationMatrix&, const G4ThreeVector&)> visit;
  visit = [&](G4VPhysicalVolume* volume, const G4RotationMatrix& parentRotation,
              const G4ThreeVector& parentTranslation) {
    if (volume->IsReplicated() || volume->IsParameterised())
      throw std::runtime_error("Source volume lookup does not yet support replicated or parameterised geometry.");
    const auto rotation = parentRotation * volume->GetObjectRotationValue();
    const auto translation = parentTranslation + parentRotation * volume->GetObjectTranslation();
    if (volume->GetName() == fConfig.volume) {
      ++matches;
      fTarget = volume;
      fRotation = rotation;
      fTranslation = translation;
    }
    auto* logical = volume->GetLogicalVolume();
    for (std::size_t i = 0; i < logical->GetNoDaughters(); ++i)
      visit(logical->GetDaughter(i), rotation, translation);
  };
  visit(world, G4RotationMatrix{}, G4ThreeVector{});
  if (matches == 0) throw std::runtime_error("Source volume not found: " + fConfig.volume);
  if (matches != 1) throw std::runtime_error("Source volume name is ambiguous: " + fConfig.volume);
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
