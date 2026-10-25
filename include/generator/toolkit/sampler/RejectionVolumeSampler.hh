#pragma once

#include "G4RotationMatrix.hh"
#include "G4ThreeVector.hh"
#include "globals.hh"

#include <cstddef>
#include <memory>

class G4Navigator;
class G4VPhysicalVolume;
class G4VSolid;
namespace cupid {
class GeometryManager;
// Uniform rejection sampling in one registered placement, excluding daughters.
class RejectionVolumeSampler {
 public:
  RejectionVolumeSampler(const G4String& volume, const GeometryManager& geometry);
  ~RejectionVolumeSampler();
  G4ThreeVector Sample();

 private:
  const GeometryManager& fGeometry;
  std::size_t fGeometryGeneration;
  G4String fVolume;
  G4VPhysicalVolume* fTarget = nullptr;
  G4VSolid* fSolid = nullptr;
  G4ThreeVector fMinimum, fMaximum, fTranslation;
  G4RotationMatrix fRotation;
  std::unique_ptr<G4Navigator> fNavigator;
};
}
