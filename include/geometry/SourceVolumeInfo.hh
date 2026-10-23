#pragma once
#include "G4RotationMatrix.hh"
#include "G4ThreeVector.hh"

class G4VPhysicalVolume;
namespace cupid {
// Non-owning placement information, valid for the current geometry generation.
struct SourceVolumeInfo {
  G4VPhysicalVolume* physicalVolume = nullptr;
  G4RotationMatrix localToWorldRotation;
  G4ThreeVector localToWorldTranslation;
};
}
