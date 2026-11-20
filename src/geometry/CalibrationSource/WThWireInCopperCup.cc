/// \file src/geometry/CalibrationSource/WThWireInCopperCup.cc
/// \brief Place a thoriated tungsten wire, copper cup and copper hole-cover tape.
///
/// GeometryManager builds this component after LMOArray. The enclosing volume
/// groups the assembly and inherits the parent's material; its three children
/// are registered separately for future source sampling. No SD is attached.
#include "geometry/CalibrationSource/WThWireInCopperCup.hh"
#include "geometry/GeometryManager.hh"
#include "materials/Materials.hh"

#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4SystemOfUnits.hh"
#include "G4Transform3D.hh"
#include "G4Tubs.hh"
#include "G4UnionSolid.hh"
#include "G4VisAttributes.hh"
#include "G4ios.hh"

#include <stdexcept>

namespace cupid {
void WThWireInCopperCup::Build(G4VPhysicalVolume* parent, const Materials& materials,
                              const GeometryParameters&, GeometryManager& geometry) const {
  const G4String sourceName = "WThWireInCopperCup";
  const auto& lmo = geometry.FindConstructionVolume("LMO0");
  const auto& mother = geometry.FindConstructionVolume(parent->GetName());
  const auto* box = dynamic_cast<const G4Box*>(lmo.physicalVolume->GetLogicalVolume()->GetSolid());
  if (!box) throw std::runtime_error("WThWireInCopperCup requires a box-shaped LMO0.");

  // Full dimensions of the laboratory calibration assembly; edit here and rebuild.
  // cupGap is measured from the bare copper hole face to LMO0's local -Z face.
  // The tape extends towards LMO0, reducing its own face gap by tapeThickness.
  const G4double cupGap = 7.5*mm;
  const G4double wireDiameter = 1.6*mm;
  const G4double wireLength = 10.*mm;
  const G4double cupDiameter = 18.*mm;
  const G4double cupHeight = 4.9*mm;
  const G4double lidThickness = 2.*mm;
  const G4double cavityDiameter = 14.6*mm;
  const G4double holeDiameter = 2.*mm;
  const G4double holeFaceThickness = 1.*mm;
  const G4double tapeRadius = 1.5*mm;
  const G4double tapeThickness = 0.005*mm;
  const G4double cavityHeight = cupHeight - lidThickness - holeFaceThickness;

  // The assembly origin is the midpoint of the cup-plus-tape axial extent.
  // Its +Z end faces the crystal. Use the actual registered LMO transform.
  const G4double halfHeight = (cupHeight + tapeThickness)/2.;
  const G4double frontZ = (cupHeight - tapeThickness)/2.;
  const auto center = lmo.localToWorldTranslation + lmo.localToWorldRotation *
      G4ThreeVector(0., 0., -box->GetZHalfLength() - cupGap - frontZ);
  const auto parentInverse = mother.localToWorldRotation.inverse();
  const G4Transform3D placement(parentInverse * lmo.localToWorldRotation,
      parentInverse * (center - mother.localToWorldTranslation));
  auto* envelopeSolid = new G4Tubs(sourceName + ".Envelope", 0., cupDiameter/2.,
                                  halfHeight, 0., 360.*deg);
  auto* envelopeLogical = new G4LogicalVolume(
      envelopeSolid, parent->GetLogicalVolume()->GetMaterial(), sourceName);
  envelopeLogical->SetVisAttributes(G4VisAttributes::GetInvisible());
  auto* envelope = new G4PVPlacement(placement, envelopeLogical, sourceName,
                                    parent->GetLogicalVolume(), false, 0);
  geometry.RegisterSourceVolume(envelope, parent);

  // The copper cup retains the three axial sections of the laboratory source.
  // The solid origin is at the centre of the closed 2 mm rear lid.
  auto* lid = new G4Tubs(sourceName + ".Lid", 0., cupDiameter/2.,
                         lidThickness/2., 0., 360.*deg);
  auto* wall = new G4Tubs(sourceName + ".Wall", cavityDiameter/2., cupDiameter/2.,
                          cavityHeight/2., 0., 360.*deg);
  auto* face = new G4Tubs(sourceName + ".HoleFace", holeDiameter/2., cupDiameter/2.,
                          holeFaceThickness/2., 0., 360.*deg);
  auto* rear = new G4UnionSolid(sourceName + ".LidWall", lid, wall, nullptr,
      {0., 0., (cupHeight - holeFaceThickness)/2.});
  auto* cup = new G4UnionSolid(sourceName + ".CopperCup.Solid", rear, face, nullptr,
      {0., 0., cupHeight - lidThickness/2. - holeFaceThickness/2.});

  auto placePart = [&](const G4String& suffix, G4VSolid* solid, G4Material* material,
                       const G4Transform3D& transform, const G4Colour& color, G4int copy) {
    const auto partName = sourceName + "." + suffix;
    auto* logical = new G4LogicalVolume(solid, material, partName);
    logical->SetVisAttributes(G4VisAttributes(color));
    auto* physical = new G4PVPlacement(transform, logical, partName,
                                      envelopeLogical, false, copy);
    geometry.RegisterSourceVolume(physical, envelope);
  };
  placePart("CopperCup", cup, materials.Get("G4_Cu"),
      G4Transform3D(G4RotationMatrix{}, {0., 0., -halfHeight + lidThickness/2.}),
      G4Colour(0.75, 0.45, 0.2, 0.35), 0);

  // Centre the wire in the 1.9 mm cavity; its cylinder axis follows LMO0 local X.
  auto* wire = new G4Tubs(sourceName + ".Wire.Solid", 0., wireDiameter/2.,
                          wireLength/2., 0., 360.*deg);
  G4RotationMatrix wireRotation;
  wireRotation.rotateY(90.*deg);
  const G4double wireZ = frontZ - holeFaceThickness - cavityHeight/2.;
  placePart("Wire", wire, materials.Get("WThWire"),
      G4Transform3D(wireRotation, {0., 0., wireZ}), G4Colour(0.4, 0.4, 0.45), 1);

  auto* tape = new G4Tubs(sourceName + ".CopperTape.Solid", 0., tapeRadius,
                          tapeThickness/2., 0., 360.*deg);
  placePart("CopperTape", tape, materials.Get("G4_Cu"),
      G4Transform3D(G4RotationMatrix{}, {0., 0., frontZ + tapeThickness/2.}),
      G4Colour(0.9, 0.6, 0.25), 2);

  const auto wireCenter = center + lmo.localToWorldRotation * G4ThreeVector(0., 0., wireZ);
  G4cout << "CUPID_CALIBRATION_SOURCE WThWireInCopperCup=" << sourceName << " target=LMO0 cup_gap_mm="
         << cupGap/mm << " wire_center_mm=" << wireCenter/mm << G4endl;
}
}  // namespace cupid
