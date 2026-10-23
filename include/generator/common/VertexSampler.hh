#pragma once
#include "generator/GeneratorConfig.hh"
#include "G4RotationMatrix.hh"
#include <memory>
#include <cstddef>

class G4Navigator;
class G4VPhysicalVolume;
class G4VSolid;
namespace cupid {
class GeometryManager;
class VertexSampler {
 public:
  VertexSampler(const GeneratorConfig&, const GeometryManager&);
  ~VertexSampler();
  G4ThreeVector Sample();

 private:
  const GeometryManager& fGeometry;
  std::size_t fGeometryGeneration;
  GeneratorConfig fConfig;
  G4VPhysicalVolume* fTarget = nullptr;
  G4VSolid* fSolid = nullptr;
  G4ThreeVector fMinimum, fMaximum, fTranslation;
  G4RotationMatrix fRotation;
  std::unique_ptr<G4Navigator> fNavigator;
};
}
