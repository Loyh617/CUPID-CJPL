/// \file include/geometry/Cryostat.hh
/// \brief Declaration: Construct the prototype cryostat plates and surrounding shells.
///
/// GeometryManager calls Build(parent, materials, parameters, geometry) when
/// Cryostat is enabled. The builder places volumes under the supplied parent
/// and registers their source-volume transforms with GeometryManager.

#pragma once
class G4VPhysicalVolume;
namespace cupid {
class Materials;
class GeometryManager;
struct GeometryParameters;
class Cryostat {
 public:
  void Build(G4VPhysicalVolume* parent, const Materials& materials,
             const GeometryParameters& parameters, GeometryManager& geometry) const;
};
}

