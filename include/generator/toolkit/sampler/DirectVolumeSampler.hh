#pragma once

#include "G4RotationMatrix.hh"
#include "G4ThreeVector.hh"
#include "globals.hh"

namespace cupid {
// Uniform volume sampling in analytic solids, without geometry navigation.
// World position = center + localToWorldRotation * local position.
class DirectVolumeSampler {
 public:
  explicit DirectVolumeSampler(const G4ThreeVector& center = {},
                               const G4RotationMatrix& localToWorldRotation = {});
  G4ThreeVector SampleBox(const G4ThreeVector& halfLengths) const;
  // The cylinder axis is local Z; its full height is twice halfLength.
  G4ThreeVector SampleCylinder(G4double radius, G4double halfLength) const;
  G4ThreeVector SampleSphere(G4double radius) const;

 private:
  G4ThreeVector ToWorld(const G4ThreeVector& local) const;
  G4ThreeVector fCenter;
  G4RotationMatrix fRotation;
};
}
