/// \file include/geometry/GeometryManager.hh
/// \brief Declaration: Build selected components and expose registered source and LMO volumes.
///
/// DetectorConstruction owns this manager. Configure components via the
/// messenger before initialization, then Build() registers their placements.
/// Generators use FindSourceVolume(); detector setup uses
/// GetLMOLogicalVolumes(). Registry references apply only to the current
/// geometry generation; the prototype supports one or two enabled LMOs.

#pragma once
#include "geometry/GeometryParameters.hh"

#include "G4RotationMatrix.hh"
#include "G4ThreeVector.hh"
#include "globals.hh"

#include <array>
#include <memory>
#include <map>
#include <cstddef>
#include <vector>

class G4LogicalVolume;
class G4VPhysicalVolume;
namespace cupid {
class GeometryMessenger;
class VGeometry;
class Materials;

// Non-owning placement information populated by GeometryManager during construction.
// The placement and local-to-World transform are valid for the current geometry generation.
struct SourceVolumeInfo {
  G4VPhysicalVolume* physicalVolume = nullptr;
  G4RotationMatrix localToWorldRotation;
  G4ThreeVector localToWorldTranslation;
};

struct LMOPlacement {
  G4ThreeVector center;  // World position of the crystal center.
  G4RotationMatrix rotation;  // Local-to-World rotation.
  G4ThreeVector halfLengths;  // Box half lengths in local coordinates.
};

class GeometryManager {
 public:
  GeometryManager();
  ~GeometryManager();
  void SetEnabled(const G4String& name, G4bool enabled);
  void SetLMOCount(G4int count);
  void List() const;
  void Build(G4VPhysicalVolume* world, const Materials& materials);
  // Construction-only registration. Register mothers before their children.
  void BeginSourceVolumes(G4VPhysicalVolume* world);
  void RegisterSourceVolume(G4VPhysicalVolume* volume, G4VPhysicalVolume* parent);
  void FinishSourceVolumes();
  // Construction-only lookup of an already registered component; master thread only.
  const SourceVolumeInfo& FindConstructionVolume(const G4String& name) const;
  // Register crystal placements in contiguous, zero-based copy-number order.
  void RegisterLMOVolume(G4VPhysicalVolume* volume);
  const std::vector<G4LogicalVolume*>& GetLMOLogicalVolumes() const;
  std::vector<LMOPlacement> GetLMOPlacements() const;
  const SourceVolumeInfo& FindSourceVolume(const G4String& name) const;
  G4VPhysicalVolume* GetWorldVolume() const;
  std::size_t GetGeometryGeneration() const { return fGeometryGeneration; }
  G4bool CheckOverlaps(G4int samples) const;
  const GeometryParameters& Parameters() const { return fParameters; }

 private:
  struct Component {
    const char* name;
    G4bool enabled;
    std::unique_ptr<VGeometry> builder;  // Owns the builder, not its placed volumes.
  };
  std::array<Component, 4> fComponents;
  GeometryParameters fParameters;
  std::map<G4String, SourceVolumeInfo> fSourceVolumes;
  std::vector<G4LogicalVolume*> fLMOLogicalVolumes;
  G4VPhysicalVolume* fWorld = nullptr;
  G4bool fRegistering = false;
  G4bool fSourceVolumesReady = false;
  std::size_t fGeometryGeneration = 0;
  std::unique_ptr<GeometryMessenger> fMessenger;
};
}

