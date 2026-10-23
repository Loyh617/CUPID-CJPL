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

