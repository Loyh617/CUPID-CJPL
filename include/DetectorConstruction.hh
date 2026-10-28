/// \file include/DetectorConstruction.hh
/// \brief Declaration: Build the World and attach sensitive detectors to the selected geometry.
///
/// Register with the run manager in main(). Construct() builds materials and
/// delegates components to GeometryManager. ConstructSDandField() attaches
/// a thread-local LMOSensitiveDetector to the registered LMO logical volumes.

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
  void ConstructSDandField() override;
  const GeometryManager& GetGeometryManager() const { return *fGeometry; }

 private:
  std::unique_ptr<GeometryManager> fGeometry;
};
}  // namespace cupid

