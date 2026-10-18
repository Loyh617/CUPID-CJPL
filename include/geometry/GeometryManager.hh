#pragma once
#include "geometry/GeometryParameters.hh"
#include "globals.hh"
#include <array>
#include <memory>

class G4LogicalVolume;
namespace cupid {
class GeometryMessenger;
class Materials;

class GeometryManager {
 public:
  GeometryManager();
  ~GeometryManager();
  void SetEnabled(const G4String& name, G4bool enabled);
  void SetCrystalCount(G4int count);
  void List() const;
  void Build(G4LogicalVolume* mother, const Materials& materials);
  G4bool CheckOverlaps(G4int samples) const;
  const GeometryParameters& Parameters() const { return parameters_; }

 private:
  using Builder = void (*)(G4LogicalVolume*, const Materials&, const GeometryParameters&);
  struct Component {
    const char* name;
    G4bool enabled;
    Builder build;
  };
  std::array<Component, 3> components_;
  GeometryParameters parameters_;
  std::unique_ptr<GeometryMessenger> messenger_;
};
}

