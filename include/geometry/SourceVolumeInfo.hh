/// \file include/geometry/SourceVolumeInfo.hh
/// \brief Declaration: Describe one registered placement and its local-to-World transform.
///
/// GeometryManager fills this non-owning structure during construction and
/// returns it through FindSourceVolume(name). Samplers use the placement and
/// transform only while that geometry generation remains valid.

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
