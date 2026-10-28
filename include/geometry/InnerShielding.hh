/// \file include/geometry/InnerShielding.hh
/// \brief Declaration: Construct the prototype inner shielding components.
///
/// GeometryManager calls Build(parent, materials, parameters, geometry) when
/// InnerShielding is enabled. The builder uses the shared dimensions and
/// registers its physical placements for source-volume lookup.

#pragma once
class G4VPhysicalVolume;
namespace cupid {
class Materials;
class GeometryManager;
struct GeometryParameters;
class InnerShielding {
 public:
  void Build(G4VPhysicalVolume* parent, const Materials& materials,
             const GeometryParameters& parameters, GeometryManager& geometry) const;
};
}

