#include "generator/common/sampler/PointVertexSampler.hh"
#include "generator/common/sampler/VolumeVertexSampler.hh"
#include "geometry/GeometryManager.hh"

#include "G4PVReplica.hh"
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
#include <limits>

namespace {
void Require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
template<class F> void Reject(F operation, const char* message) {
  bool rejected = false;
  try { operation(); }
  catch (const std::runtime_error&) { rejected = true; }
  Require(rejected, message);
}
}
int main() {
  try {
    G4Random::setTheSeed(123456);
    auto* material = G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic");
    auto* worldLV = new G4LogicalVolume(new G4Box("WorldSolid", 500*mm, 500*mm, 500*mm), material, "WorldLV");
    auto* world = new G4PVPlacement(nullptr, {}, worldLV, "World", nullptr, false, 0);
    cupid::GeometryManager geometry;
    Reject([&] { geometry.FindSourceVolume("World"); }, "Uninitialized registry accepted");
    geometry.BeginSourceVolumes(world);
    auto* holderLV = new G4LogicalVolume(new G4Box("HolderSolid", 60*mm, 60*mm, 60*mm), material, "HolderLV");
    auto* rotation = new G4RotationMatrix;
    rotation->rotateZ(30*deg);
    auto* holder = new G4PVPlacement(rotation, {100*mm, 70*mm, 30*mm}, holderLV, "Holder", worldLV, false, 0);
    geometry.RegisterSourceVolume(holder, world);
    auto* box = new G4Box("TargetBox", 20*mm, 10*mm, 8*mm);
    auto* hole = new G4Box("Hole", 3*mm, 3*mm, 20*mm);
    auto* targetSolid = new G4SubtractionSolid("TargetSolid", box, hole);
    auto* targetLV = new G4LogicalVolume(targetSolid, material, "TargetLV");
    auto* targetRotation = new G4RotationMatrix;
    targetRotation->rotateY(20*deg);
    auto* target = new G4PVPlacement(targetRotation, {5*mm, 2*mm, 0}, targetLV, "Target", holderLV, false, 0);
    Reject([&] { geometry.RegisterSourceVolume(target, world); }, "Wrong parent accepted");
    geometry.RegisterSourceVolume(target, holder);
    Reject([&] { geometry.RegisterSourceVolume(target, holder); }, "Duplicate registration accepted");
    auto* daughterLV = new G4LogicalVolume(new G4Box("DaughterSolid", 2*mm, 2*mm, 2*mm), material, "DaughterLV");
    new G4PVPlacement(nullptr, {12*mm, 0, 0}, daughterLV, "Daughter", targetLV, false, 0);
    new G4PVPlacement(nullptr, {-100*mm, 0, 0}, daughterLV, "Duplicate", worldLV, false, 1);
    new G4PVPlacement(nullptr, {-120*mm, 0, 0}, daughterLV, "Duplicate", worldLV, false, 2);
    Reject([&] { geometry.FindSourceVolume("Target"); }, "Unfinished registry accepted");
    geometry.FinishSourceVolumes();
    Require(geometry.FindSourceVolume("Target").physicalVolume == target, "Wrong registered placement");
    Reject([&] { geometry.RegisterSourceVolume(target, holder); }, "Closed registry changed");
    G4GeometryManager::GetInstance()->CloseGeometry();
    auto* trackingNavigator = G4TransportationManager::GetTransportationManager()->GetNavigatorForTracking();
    trackingNavigator->SetWorldVolume(world);
    G4Navigator verify;
    verify.SetWorldVolume(world);

    cupid::VolumeVertexSampler sampler("Target", geometry);
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

    for (const auto* name : {"Missing", "Duplicate", "Daughter"}) {
      Reject([&] { cupid::VolumeVertexSampler invalid(name, geometry); },
             "Missing or unregistered source volume was accepted");
    }
    Reject([&] { cupid::VolumeVertexSampler invalid("", geometry); }, "Empty volume accepted");
    // The explicit volume parameter must select different actual placements.
    cupid::VolumeVertexSampler holderSampler("Holder", geometry);
    for (int i = 0; i < 200; ++i)
      Require(verify.LocateGlobalPointAndSetup(holderSampler.Sample(), nullptr, false) == holder,
              "Explicit volume parameter did not select Holder");

    const G4ThreeVector fixedPoint{100*mm, 70*mm, 30*mm};
    G4Random::setTheSeed(7654321);
    const auto expectedRandom = G4UniformRand();
    G4Random::setTheSeed(7654321);
    cupid::PointVertexSampler pointSampler(fixedPoint, geometry);
    for (int i = 0; i < 100; ++i)
      Require(pointSampler.Sample() == fixedPoint, "Point source changed");
    Require(G4UniformRand() == expectedRandom, "Point sampling consumed random numbers");
    for (const auto& point : {G4ThreeVector{500*mm, 0, 0}, G4ThreeVector{501*mm, 0, 0},
                              G4ThreeVector{std::numeric_limits<double>::infinity(), 0, 0},
                              G4ThreeVector{0, std::numeric_limits<double>::quiet_NaN(), 0}})
      Reject([&] { cupid::PointVertexSampler invalid(point, geometry); }, "Invalid fixed point accepted");

    // A rebuild clears the registry and invalidates existing samplers.
    G4GeometryManager::GetInstance()->OpenGeometry();
    geometry.BeginSourceVolumes(world);
    Reject([&] { sampler.Sample(); }, "Stale volume sampler accepted");
    Reject([&] { pointSampler.Sample(); }, "Stale point sampler accepted");
    Reject([&] { geometry.RegisterSourceVolume(target, holder); }, "Unregistered parent accepted");
    geometry.FinishSourceVolumes();
    Reject([&] { geometry.FindSourceVolume("Target"); }, "Registry retained old entries");

    geometry.BeginSourceVolumes(world);
    geometry.RegisterSourceVolume(holder, world);
    geometry.RegisterSourceVolume(target, holder);
    new G4PVPlacement(nullptr, {-200*mm, 0, 0}, holderLV, "OtherHolder", worldLV, false, 1);
    Reject([&] { geometry.FinishSourceVolumes(); }, "Repeated mother logical volume accepted");
    Reject([&] { geometry.FindSourceVolume("Target"); }, "Failed registry exposed for sampling");

    geometry.BeginSourceVolumes(world);
    auto* replicaLV = new G4LogicalVolume(new G4Box("ReplicaSolid", 1*mm, 1*mm, 1*mm), material, "ReplicaLV");
    auto* replicaMotherLV = new G4LogicalVolume(new G4Box("ReplicaMotherSolid", 2*mm, 1*mm, 1*mm), material, "ReplicaMotherLV");
    auto* replicaMother = new G4PVPlacement(nullptr, {200*mm, 0, 0}, replicaMotherLV, "ReplicaMother", worldLV, false, 0);
    geometry.RegisterSourceVolume(replicaMother, world);
    auto* replica = new G4PVReplica("Replica", replicaLV, replicaMotherLV, kXAxis, 2, 2*mm);
    Reject([&] { geometry.RegisterSourceVolume(replica, replicaMother); }, "Replica accepted");

    geometry.BeginSourceVolumes(world);
    auto* duplicate = new G4PVPlacement(nullptr, {-140*mm, 0, 0}, daughterLV, "Duplicate", worldLV, false, 3);
    geometry.RegisterSourceVolume(duplicate, world);
    Reject([&] { geometry.FinishSourceVolumes(); }, "Unregistered name collision accepted");
    std::cout << "PASS nested transforms, sampling, registry validation and rebuild invalidation\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
