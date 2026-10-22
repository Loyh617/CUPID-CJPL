#pragma once

#include "G4VUserDetectorConstruction.hh"
#include <memory>

namespace cupid {
class GeometryManager;

class DetectorConstruction final : public G4VUserDetectorConstruction {
 public:
  DetectorConstruction();
  ~DetectorConstruction() override;
  G4VPhysicalVolume* Construct() override;

 private:
  std::unique_ptr<GeometryManager> fGeometry;
};
}  // namespace cupid

