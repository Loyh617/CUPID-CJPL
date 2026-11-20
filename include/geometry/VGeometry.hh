/// \file include/geometry/VGeometry.hh
/// \brief Define the common construction interface for geometry components.
///
/// Derive from VGeometry and implement Build() with override. GeometryManager
/// owns the builders and invokes enabled components in registration order during
/// master geometry construction. DetectorConstruction remains the Geant4 entry point.
#pragma once

class G4VPhysicalVolume;
namespace cupid {
class Materials;
class GeometryManager;
struct GeometryParameters;

class VGeometry {
 public:
  virtual ~VGeometry() = default;

  // Place volumes beneath the actual parent and register mothers before children.
  // Arguments are borrowed for this call; builders do not own Geant4 volumes.
  // Read dimensions in Geant4 units. const applies to the builder, not the registry.
  // Attach sensitive detectors later in DetectorConstruction::ConstructSDandField().
  virtual void Build(G4VPhysicalVolume* parent, const Materials& materials,
                     const GeometryParameters& parameters, GeometryManager& geometry) const = 0;
};
}  // namespace cupid
