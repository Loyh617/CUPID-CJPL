#include "generator/common/VertexSampler.hh"
#include "G4Box.hh"
#include "G4GeometryManager.hh"
#include "G4LogicalVolume.hh"
#include "G4Navigator.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4SubtractionSolid.hh"
#include "G4TransportationManager.hh"
#include "Randomize.hh"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
}
int main() {
  try {
    G4Random::setTheSeed(123456);
    auto* material = G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic");
    auto* worldLV = new G4LogicalVolume(new G4Box("WorldSolid", 500*mm, 500*mm, 500*mm), material, "WorldLV");
    auto* world = new G4PVPlacement(nullptr, {}, worldLV, "World", nullptr, false, 0);
    auto* holderLV = new G4LogicalVolume(new G4Box("HolderSolid", 60*mm, 60*mm, 60*mm), material, "HolderLV");
    auto* rotation = new G4RotationMatrix;
    rotation->rotateZ(30*deg);
    auto* holder = new G4PVPlacement(rotation, {100*mm, 70*mm, 30*mm}, holderLV, "Holder", worldLV, false, 0);
    auto* box = new G4Box("TargetBox", 20*mm, 10*mm, 8*mm);
    auto* hole = new G4Box("Hole", 3*mm, 3*mm, 20*mm);
    auto* targetSolid = new G4SubtractionSolid("TargetSolid", box, hole);
    auto* targetLV = new G4LogicalVolume(targetSolid, material, "TargetLV");
    auto* targetRotation = new G4RotationMatrix;
    targetRotation->rotateY(20*deg);
    auto* target = new G4PVPlacement(targetRotation, {5*mm, 2*mm, 0}, targetLV, "Target", holderLV, false, 0);
    auto* daughterLV = new G4LogicalVolume(new G4Box("DaughterSolid", 2*mm, 2*mm, 2*mm), material, "DaughterLV");
    new G4PVPlacement(nullptr, {12*mm, 0, 0}, daughterLV, "Daughter", targetLV, false, 0);
    new G4PVPlacement(nullptr, {-100*mm, 0, 0}, daughterLV, "Duplicate", worldLV, false, 1);
    new G4PVPlacement(nullptr, {-120*mm, 0, 0}, daughterLV, "Duplicate", worldLV, false, 2);
    G4GeometryManager::GetInstance()->CloseGeometry();
    auto* trackingNavigator = G4TransportationManager::GetTransportationManager()->GetNavigatorForTracking();
    trackingNavigator->SetWorldVolume(world);
    G4Navigator verify;
    verify.SetWorldVolume(world);

    cupid::GeneratorConfig config;
    config.positionMode = "volume";
    config.volume = "Target";
    cupid::VertexSampler sampler(config);
    G4double sumX = 0.;
    for (int i = 0; i < 12000; ++i) {
      const auto point = sampler.Sample();
      Require(verify.LocateGlobalPointAndSetup(point, nullptr, false) == target,
              "Sample is in a hole, daughter or wrong placement");
      const auto inHolder = holder->GetObjectRotationValue().inverse() * (point - holder->GetObjectTranslation());
      const auto local = target->GetObjectRotationValue().inverse() * (inHolder - target->GetObjectTranslation());
      Require(targetSolid->Inside(local) == kInside, "Wrong nested placement transform");
      sumX += local.x()/mm;
    }
    // The asymmetric daughter removes 64 mm^3 centered at x=12 mm.
    const double expectedMean = -12.*64./(40.*20.*16. - 6.*6.*16. - 64.);
    Require(std::abs(sumX/12000 - expectedMean) < 0.5, "Volume sampling mean is biased");

    for (const auto* name : {"Missing", "Duplicate"}) {
      config.volume = name;
      bool rejected = false;
      try { cupid::VertexSampler invalid(config); }
      catch (const std::runtime_error&) { rejected = true; }
      Require(rejected, "Missing or ambiguous source volume was accepted");
    }
    std::cout << "PASS nested rotated volume, subtraction hole, daughter exclusion and ambiguous names\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
