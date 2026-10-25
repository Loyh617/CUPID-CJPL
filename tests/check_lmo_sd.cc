#include "DetectorConstruction.hh"
#include "PhysicsList.hh"
#include "geometry/GeometryManager.hh"
#include "LMOSensitiveDetector.hh"

#include "G4Event.hh"
#include "G4LogicalVolume.hh"
#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4RunManager.hh"
#include "G4RunManagerFactory.hh"
#include "G4SDManager.hh"
#include "G4Step.hh"
#include "G4SystemOfUnits.hh"
#include "G4UImanager.hh"
#include "G4UserEventAction.hh"
#include "G4UserSteppingAction.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VUserActionInitialization.hh"
#include "G4VUserPrimaryGeneratorAction.hh"
#include "Randomize.hh"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
std::atomic<int> checkedEvents{0};
std::atomic<int> failedChecks{0};

void Check(bool condition, const char* message) {
  if (!condition) {
    ++failedChecks;
    G4cerr << "LMO_SD_FAIL: " << message << G4endl;
  }
}

class TestPrimary final : public G4VUserPrimaryGeneratorAction {
 public:
  explicit TestPrimary(int count) : fCount(count) {}

  void GeneratePrimaries(G4Event* event) override {
    const auto mode = event->GetEventID() % 4;
    fGun.SetParticleDefinition(G4ParticleTable::GetParticleTable()->FindParticle(
        mode == 3 ? "geantino" : "e-"));
    fGun.SetParticleEnergy(1. * MeV);
    fGun.SetParticleMomentumDirection({0., 0., 1.});
    if (mode == 2 && fCount == 2) {
      Emit(event, 0);
      Emit(event, 1);
    } else {
      Emit(event, fCount == 2 ? mode % 2 : 0);
    }
  }

 private:
  void Emit(G4Event* event, int id) {
    const auto x = fCount == 2 ? (id == 0 ? -25. : 25.) * mm : 0.;
    fGun.SetParticlePosition({x, -350. * mm, 0.});
    fGun.GeneratePrimaryVertex(event);
  }
  int fCount;
  G4ParticleGun fGun{1};
};

class TestEvent final : public G4UserEventAction {
 public:
  TestEvent(const cupid::DetectorConstruction& detector, int count)
      : fDetector(detector), fCount(count) {}

  void BeginOfEventAction(const G4Event*) override {
    fReference.clear();
    const auto& geometry = fDetector.GetGeometryManager();
    const auto& lmos = geometry.GetLMOLogicalVolumes();
    Check(lmos.size() == static_cast<std::size_t>(fCount), "crystal registry size");
    Check(!geometry.GetWorldVolume()->GetLogicalVolume()->GetSensitiveDetector(),
          "world must not be sensitive");
    auto* sd = G4SDManager::GetSDMpointer()->FindSensitiveDetector(
        cupid::LMOSensitiveDetector::DetectorName, false);
    Check((sd != nullptr) == (fCount > 0), "SD exists only for enabled crystals");
    for (int lmoId = 0; lmoId < fCount; ++lmoId) {
      const G4String name = "LMO" + std::to_string(lmoId);
      auto* logical = lmos[lmoId];
      auto* physical = geometry.FindSourceVolume(name).physicalVolume;
      Check(logical->GetName() == name, "LMO logical-volume name");
      Check(physical->GetName() == name && physical->GetLogicalVolume() == logical,
            "LMO physical-volume name and source lookup");
      Check(physical->GetCopyNo() == lmoId, "LMO copy number matches ID");
      Check(logical->GetSensitiveDetector() == sd, "all crystals use this thread's SD");
      Check(dynamic_cast<cupid::LMOSensitiveDetector*>(sd) != nullptr, "SD type");
    }
  }

  void AddReference(const G4Step* step) {
    const auto energy = step->GetTotalEnergyDeposit();
    if (energy == 0.) return;
    auto* physical = step->GetPreStepPoint()->GetPhysicalVolume();
    const auto& lmos = fDetector.GetGeometryManager().GetLMOLogicalVolumes();
    // Identify the crystal by its registered logical-volume index, independently of copy number.
    const auto found = std::find(lmos.begin(), lmos.end(), physical->GetLogicalVolume());
    if (found != lmos.end())
      fReference[static_cast<int>(std::distance(lmos.begin(), found))] += energy;
  }

  void EndOfEventAction(const G4Event* event) override {
    ++checkedEvents;
    Check(!event->IsAborted(), "event completed");
    auto* collections = event->GetHCofThisEvent();
    if (fCount == 0) {
      Check(fReference.empty(), "world-only reference empty");
      Check(!collections || collections->GetNumberOfCollections() == 0, "world-only has no hits");
      return;
    }
    const auto id = G4SDManager::GetSDMpointer()->GetCollectionID(
        cupid::LMOSensitiveDetector::CollectionFullName);
    Check(id >= 0 && collections, "collection registered and attached to event");
    if (id < 0 || !collections) return;
    auto* hits = dynamic_cast<G4THitsMap<G4double>*>(collections->GetHC(id));
    Check(hits != nullptr, "collection type");
    if (!hits) return;
    Check(hits->entries() == fReference.size(), "sparse collection size");
    for (const auto& [lmoId, energy] : fReference) {
      const auto* recorded = (*hits)[lmoId];
      Check(recorded && std::abs(*recorded - energy) < 1.e-10 * MeV,
            "SD matches independent sum of all particle steps");
    }
    G4double total = 0.;
    for (const auto& [lmoId, energy] : *hits->GetMap()) {
      Check(lmoId >= 0 && lmoId < fCount, "valid crystal ID");
      Check(energy && *energy > 0., "positive sparse entry");
      if (energy) total += *energy;
    }
    const auto mode = event->GetEventID() % 4;
    if (mode == 3) {
      Check(hits->entries() == 0, "geantino event has no residual energy");
    } else {
      const auto target = fCount == 2 ? mode % 2 : 0;
      Check((*hits)[target] && *(*hits)[target] > 0., "target crystal recorded");
      const auto primaries = mode == 2 && fCount == 2 ? 2 : 1;
      Check(total > 0. && total <= primaries * MeV + 1.e-8 * MeV,
            "event energy is positive and not double counted");
      if (primaries == 2)
        Check((*hits)[0] && (*hits)[1], "both primaries recorded in the same event");
    }
  }

 private:
  const cupid::DetectorConstruction& fDetector;
  int fCount;
  std::map<int, G4double> fReference;
};

class TestStep final : public G4UserSteppingAction {
 public:
  explicit TestStep(TestEvent& event) : fEvent(event) {}
  void UserSteppingAction(const G4Step* step) override { fEvent.AddReference(step); }
 private:
  TestEvent& fEvent;
};

class TestActions final : public G4VUserActionInitialization {
 public:
  TestActions(const cupid::DetectorConstruction& detector, int count)
      : fDetector(detector), fCount(count) {}
  void Build() const override {
    auto* event = new TestEvent(fDetector, fCount);
    SetUserAction(new TestPrimary(fCount));
    SetUserAction(event);
    SetUserAction(new TestStep(*event));
  }
 private:
  const cupid::DetectorConstruction& fDetector;
  int fCount;
};
}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc != 3) throw std::invalid_argument("Usage: check-lmo-sd serial|mt count");
    const auto count = std::stoi(argv[2]);
    if (count < 0 || count > 2) throw std::invalid_argument("count must be 0, 1 or 2");
    const auto type = std::string(argv[1]) == "mt" ? G4RunManagerType::MTOnly
                                                  : G4RunManagerType::SerialOnly;
    G4Random::setTheSeed(739127);
    std::unique_ptr<G4RunManager> run(G4RunManagerFactory::CreateRunManager(type, 2));
    auto* detector = new cupid::DetectorConstruction;
    run->SetUserInitialization(detector);
    run->SetUserInitialization(new cupid::PhysicsList(0));
    run->SetUserInitialization(new TestActions(*detector, count));
    auto* ui = G4UImanager::GetUIpointer();
    if (count > 0) {
      Check(ui->ApplyCommand("/cupid/geometry/enable LMOArray") == 0, "enable crystals");
      Check(ui->ApplyCommand("/cupid/geometry/lmoCount " + std::to_string(count)) == 0,
            "set crystal count");
    }
    run->Initialize();
    // Repeating the hook must neither register nor attach a duplicate SD.
    detector->ConstructSDandField();
    run->BeamOn(16);
    run->BeamOn(12);
    Check(checkedEvents == 28, "all events in both runs checked");
    if (failedChecks != 0) return 1;
    G4cout << "LMO_SD_PASS mode=" << argv[1] << " lmos=" << count
           << " events=" << checkedEvents << G4endl;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
