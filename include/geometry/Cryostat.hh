#pragma once
class G4LogicalVolume;
namespace cupid {
class Materials;
struct GeometryParameters;
class Cryostat {
 public:
  void Build(G4LogicalVolume* mother, const Materials& materials,
             const GeometryParameters& parameters) const;
};
}

