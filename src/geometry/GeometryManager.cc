/// \file src/geometry/GeometryManager.cc
/// \brief Implementation: Build selected components and expose registered source and LMO volumes.
///
/// DetectorConstruction owns this manager. Configure components via the
/// messenger before initialization, then Build() registers their placements.
/// Generators use FindSourceVolume(); detector setup uses
/// GetLMOLogicalVolumes(). Registry references apply only to the current
/// geometry generation; the prototype supports one or two enabled LMOs.

#include "geometry/GeometryManager.hh"
#include "geometry/GeometryMessenger.hh"
#include "geometry/Cryostat.hh"
#include "geometry/InnerShielding.hh"
#include "geometry/LMOArray.hh"

#include "G4PhysicalVolumeStore.hh"
#include "G4VPhysicalVolume.hh"
#include "G4ios.hh"
#include "G4StateManager.hh"
#include "G4Threading.hh"

#include <algorithm>
#include <stdexcept>

namespace cupid {
// Components start disabled. Select them with /cupid/geometry/enable before initialization.
GeometryManager::GeometryManager()
    : fComponents{{
          {"Cryostat", false, [](auto* m, const auto& a, const auto& p, auto& g) { Cryostat{}.Build(m,a,p,g); }},
          {"InnerShielding", false, [](auto* m, const auto& a, const auto& p, auto& g) { InnerShielding{}.Build(m,a,p,g); }},
          {"LMOArray", false, [](auto* m, const auto& a, const auto& p, auto& g) { LMOArray{}.Build(m,a,p,g); }}
      }}, fMessenger(std::make_unique<GeometryMessenger>(*this)) {}
GeometryManager::~GeometryManager() = default;

void GeometryManager::SetEnabled(const G4String& name, G4bool enabled) {
  for (auto& component : fComponents) {
    if (name == component.name) { component.enabled = enabled; return; }
  }
  throw std::invalid_argument("Unknown geometry component '" + name +
      "'. Available: Cryostat InnerShielding LMOArray");
}
void GeometryManager::SetLMOCount(G4int count) {
  // This bound reflects the checked prototype layout; increasing it requires geometry validation.
  if (count < 1 || count > 2) throw std::invalid_argument("lmoCount must be 1 or 2 in v0.1");
  fParameters.lmoCount = count;
}
void GeometryManager::List() const {
  G4cout << "CUPID_GEOMETRY model=prototype-v0.1 environment=vacuum axis=Y" << G4endl;
  for (const auto& component : fComponents) {
    G4cout << "  " << component.name << "=" << (component.enabled ? "on" : "off") << G4endl;
  }
  G4cout << "  lmoCount=" << fParameters.lmoCount << G4endl;
}
void GeometryManager::Build(G4VPhysicalVolume* world, const Materials& materials) {
  BeginSourceVolumes(world);
  List();
  G4int enabled = 0;
  for (const auto& component : fComponents) {
    if (component.enabled) {
      component.build(world, materials, fParameters, *this);
      ++enabled;
    }
  }
  FinishSourceVolumes();
  if (!enabled) G4cout << "CUPID_NOTICE: no components enabled; world-only geometry." << G4endl;
}
void GeometryManager::BeginSourceVolumes(G4VPhysicalVolume* world) {
  const auto state = G4StateManager::GetStateManager()->GetCurrentState();
  if (G4Threading::IsWorkerThread() || (state != G4State_PreInit && state != G4State_Init))
    throw std::runtime_error("Source volumes may only be registered during master geometry construction.");
  fSourceVolumesReady = false;
  fRegistering = false;
  fSourceVolumes.clear();
  fLMOLogicalVolumes.clear();
  fWorld = nullptr;
  ++fGeometryGeneration;
  if (!world || world->GetMotherLogical() || world->IsReplicated() || world->IsParameterised() ||
      world->GetObjectTranslation() != G4ThreeVector{} ||
      world->GetObjectRotationValue() != G4RotationMatrix{})
    throw std::runtime_error("Source registry requires an untransformed, ordinary World placement.");
  fWorld = world;
  fSourceVolumes.emplace(world->GetName(), SourceVolumeInfo{world, {}, {}});
  fRegistering = true;
}

void GeometryManager::RegisterSourceVolume(G4VPhysicalVolume* volume, G4VPhysicalVolume* parent) {
  if (G4Threading::IsWorkerThread() || !fRegistering)
    throw std::runtime_error("Source volume registration is closed.");
  if (!volume || !parent || volume->IsReplicated() || volume->IsParameterised())
    throw std::runtime_error("Source registration requires ordinary physical placements.");
  const auto found = fSourceVolumes.find(parent->GetName());
  if (found == fSourceVolumes.end() || found->second.physicalVolume != parent ||
      volume->GetMotherLogical() != parent->GetLogicalVolume())
    throw std::runtime_error("Source volume requires its actual registered parent: " + volume->GetName());
  const auto& mother = found->second;
  SourceVolumeInfo info;
  info.physicalVolume = volume;
  info.localToWorldRotation = mother.localToWorldRotation * volume->GetObjectRotationValue();
  info.localToWorldTranslation = mother.localToWorldTranslation +
                                mother.localToWorldRotation * volume->GetObjectTranslation();
  if (!fSourceVolumes.emplace(volume->GetName(), info).second)
    throw std::runtime_error("Source volume name is ambiguous: " + volume->GetName());
}

void GeometryManager::RegisterLMOVolume(G4VPhysicalVolume* volume) {
  if (G4Threading::IsWorkerThread() || !fRegistering)
    throw std::runtime_error("LMO registration is closed.");
  if (!volume) throw std::invalid_argument("Cannot register a null crystal placement.");
  const auto found = fSourceVolumes.find(volume->GetName());
  if (found == fSourceVolumes.end() || found->second.physicalVolume != volume)
    throw std::runtime_error("Register the crystal source volume before its scoring volume.");
  auto* logical = volume->GetLogicalVolume();
  if (volume->GetCopyNo() != static_cast<G4int>(fLMOLogicalVolumes.size()) ||
      std::find(fLMOLogicalVolumes.begin(), fLMOLogicalVolumes.end(), logical) !=
          fLMOLogicalVolumes.end())
    throw std::runtime_error("LMOs require distinct logical volumes and contiguous copy numbers from zero.");
  fLMOLogicalVolumes.push_back(logical);
}

const std::vector<G4LogicalVolume*>& GeometryManager::GetLMOLogicalVolumes() const {
  if (!fSourceVolumesReady)
    throw std::runtime_error("LMO lookup requires initialized geometry.");
  return fLMOLogicalVolumes;
}

void GeometryManager::FinishSourceVolumes() {
  if (G4Threading::IsWorkerThread() || !fRegistering)
    throw std::runtime_error("Source volume registration is closed.");
  // Validate identities once, including placements not exposed as source targets.
  std::map<G4String, std::size_t> names;
  std::map<G4LogicalVolume*, std::size_t> placements;
  for (auto* volume : *G4PhysicalVolumeStore::GetInstance()) {
    ++names[volume->GetName()];
    ++placements[volume->GetLogicalVolume()];
  }
  for (const auto& entry : fSourceVolumes) {
    auto* volume = entry.second.physicalVolume;
    if (names[entry.first] != 1)
      throw std::runtime_error("Source volume name is ambiguous: " + entry.first);
    auto* mother = volume->GetMotherLogical();
    if (mother && placements[mother] != 1)
      throw std::runtime_error("Source registration does not support multiply placed mother logical volumes: " + entry.first);
  }
  fRegistering = false;
  fSourceVolumesReady = true;
}

const SourceVolumeInfo& GeometryManager::FindSourceVolume(const G4String& name) const {
  if (!fSourceVolumesReady)
    throw std::runtime_error("Source sampling requires initialized geometry.");
  const auto found = fSourceVolumes.find(name);
  if (found == fSourceVolumes.end()) throw std::runtime_error("Source volume not found: " + name);
  return found->second;
}

G4VPhysicalVolume* GeometryManager::GetWorldVolume() const {
  if (!fSourceVolumesReady)
    throw std::runtime_error("Source sampling requires initialized geometry.");
  return fWorld;
}

G4bool GeometryManager::CheckOverlaps(G4int samples) const {
  G4bool failed = false;
  G4int checked = 0;
  for (auto* volume : *G4PhysicalVolumeStore::GetInstance()) {
    if (!volume->GetMotherLogical()) continue;
    ++checked;
    // Evaluate every volume even after a previous failure.
    if (volume->CheckOverlaps(samples, 0., true, 3)) failed = true;
  }
  G4cout << "CUPID_GEOMETRY_CHECK volumes=" << checked << " samples=" << samples
         << " result=" << (failed ? "FAIL" : "PASS") << G4endl;
  return !failed;
}
}

