/// \file include/generator/toolkit/sampler/DirectSurfaceSampler.hh
/// \brief Declaration: Sample uniformly by area on rectangles, spheres and curved hemispheres.
///
/// Supply a center and local-to-World rotation, then call SampleRectangle(),
/// SampleSphere() or SampleHemisphere(). Rectangles use XY half lengths;
/// hemispheres occupy local z >= 0 without the base disk. Returned points
/// have no boundary offset, navigation check or sampled emission direction.

#pragma once

#include "G4RotationMatrix.hh"
#include "G4ThreeVector.hh"
#include "globals.hh"

namespace cupid {
// Uniform area sampling, without geometry navigation or boundary offsets.
// World position = center + localToWorldRotation * local position.
class DirectSurfaceSampler {
 public:
  explicit DirectSurfaceSampler(const G4ThreeVector& center = {},
                                const G4RotationMatrix& localToWorldRotation = {});
  // The rectangle lies in local XY with z = 0; arguments are half lengths.
  G4ThreeVector SampleRectangle(G4double halfX, G4double halfY) const;
  G4ThreeVector SampleSphere(G4double radius) const;
  // Curved surface in local z >= 0; excludes the flat base disk.
  G4ThreeVector SampleHemisphere(G4double radius) const;

 private:
  G4ThreeVector ToWorld(const G4ThreeVector& local) const;
  G4ThreeVector fCenter;
  G4RotationMatrix fRotation;
};
}
