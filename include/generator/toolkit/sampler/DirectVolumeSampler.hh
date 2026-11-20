/// \file include/generator/toolkit/sampler/DirectVolumeSampler.hh
/// \brief Declaration: Sample uniformly by volume in analytic boxes, tubes and spheres.
///
/// A concrete generator supplies a center and local-to-World rotation, then
/// calls SampleBox(), SampleTubs() or SampleSphere() in Geant4 units.
/// Static overloads instead read a named placement from initialized GeometryManager.
/// Box dimensions and tube height use half lengths; radii are not diameters.
/// Points are in World coordinates. Geometry overloads reject daughter volumes.

#pragma once

#include "G4PhysicalConstants.hh"
#include "G4RotationMatrix.hh"
#include "G4ThreeVector.hh"
#include "globals.hh"

namespace cupid {
class GeometryManager;
// Uniform volume sampling in analytic solids, without geometry navigation.
// World position = center + localToWorldRotation * local position.
class DirectVolumeSampler {
 public:
  explicit DirectVolumeSampler(const G4ThreeVector& center = {},
                               const G4RotationMatrix& localToWorldRotation = {});
  G4ThreeVector SampleBox(const G4ThreeVector& halfLengths) const;
  // Radii: 0 <= innerRadius < outerRadius. Tube axis: local Z.
  // Lengths use Geant4 units. Angles use radians (or explicit deg units).
  // Phi: finite start, 0 < deltaPhi <= 2*pi. Theta: 0 <= startTheta < pi,
  // 0 < deltaTheta <= pi - startTheta. Defaults cover the full angular range.
  G4ThreeVector SampleTubs(G4double innerRadius, G4double outerRadius,
                          G4double halfLength, G4double startPhi = 0.,
                          G4double deltaPhi = twopi) const;
  G4ThreeVector SampleSphere(G4double innerRadius, G4double outerRadius,
                            G4double startPhi = 0., G4double deltaPhi = twopi,
                            G4double startTheta = 0., G4double deltaTheta = pi) const;

  // Read the registered placement's frame and dimensions on every call.
  // Geometry must be initialized and stable during sampling; no pointers are cached.
  // Box accepts G4Box; Tubs accepts G4Tubs; Sphere accepts G4Sphere or G4Orb.
  // Daughter volumes are rejected: use RejectionVolumeSampler to exclude them.
  static G4ThreeVector SampleBox(const G4String& volume, const GeometryManager& geometry);
  static G4ThreeVector SampleTubs(const G4String& volume, const GeometryManager& geometry);
  static G4ThreeVector SampleSphere(const G4String& volume, const GeometryManager& geometry);

 private:
  G4ThreeVector ToWorld(const G4ThreeVector& local) const;
  G4ThreeVector fCenter;
  G4RotationMatrix fRotation;
};
}
