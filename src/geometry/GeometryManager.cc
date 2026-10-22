#include "geometry/GeometryManager.hh"
#include "geometry/GeometryMessenger.hh"
#include "geometry/Cryostat.hh"
#include "geometry/InnerShielding.hh"
#include "geometry/CrystalArray.hh"
#include "G4PhysicalVolumeStore.hh"
#include "G4VPhysicalVolume.hh"
#include "G4ios.hh"
#include <stdexcept>

namespace cupid {
GeometryManager::GeometryManager()
    : fComponents{{
          {"Cryostat", false, [](auto* m, const auto& a, const auto& p) { Cryostat{}.Build(m,a,p); }},
          {"InnerShielding", false, [](auto* m, const auto& a, const auto& p) { InnerShielding{}.Build(m,a,p); }},
          {"CrystalArray", false, [](auto* m, const auto& a, const auto& p) { CrystalArray{}.Build(m,a,p); }}
      }}, fMessenger(std::make_unique<GeometryMessenger>(*this)) {}
GeometryManager::~GeometryManager() = default;

void GeometryManager::SetEnabled(const G4String& name, G4bool enabled) {
  for (auto& component : fComponents) {
    if (name == component.name) { component.enabled = enabled; return; }
  }
  throw std::invalid_argument("Unknown geometry component '" + name +
      "'. Available: Cryostat InnerShielding CrystalArray");
}
void GeometryManager::SetCrystalCount(G4int count) {
  if (count < 1 || count > 2) throw std::invalid_argument("crystalCount must be 1 or 2 in v0.1");
  fParameters.crystalCount = count;
}
void GeometryManager::List() const {
  G4cout << "CUPID_GEOMETRY model=prototype-v0.1 environment=vacuum axis=Y" << G4endl;
  for (const auto& component : fComponents) {
    G4cout << "  " << component.name << "=" << (component.enabled ? "on" : "off") << G4endl;
  }
  G4cout << "  crystalCount=" << fParameters.crystalCount << G4endl;
}
void GeometryManager::Build(G4LogicalVolume* mother, const Materials& materials) {
  List();
  G4int enabled = 0;
  for (const auto& component : fComponents) {
    if (component.enabled) {
      component.build(mother, materials, fParameters);
      ++enabled;
    }
  }
  if (!enabled) G4cout << "CUPID_NOTICE: no components enabled; world-only geometry." << G4endl;
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

