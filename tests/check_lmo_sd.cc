#include "DetectorConstruction.hh"
#include "DetectorHit.hh"
#include "EventAction.hh"
#include "TrackingAction.hh"
#include "PhysicsList.hh"
#include "geometry/GeometryManager.hh"
#include "LMOSensitiveDetector.hh"

#include "G4Event.hh"
#include "G4LogicalVolume.hh"
#include "G4ParticleDefinition.hh"
#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4RunManager.hh"
#include "G4RunManagerFactory.hh"
#include "G4SDManager.hh"
#include "G4Step.hh"
#include "G4Track.hh"
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
std::atomic<int> secondaryDeposits{0};

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
    const auto mode = event->GetEventID() % 6;
    const G4String particle = mode == 3 ? "geantino" : mode == 4 ? "gamma" : mode == 5 ? "mu-" : "e-";
    fGun.SetParticleDefinition(G4ParticleTable::GetParticleTable()->FindParticle(particle));
    fGun.SetParticleEnergy((mode == 4 ? 2.6145 : mode == 5 ? 100. : 1.) * MeV);
    fGun.SetParticleMomentumDirection({0., 0., 1.});
    if (mode == 2) {
      Emit(event, 0);
      Emit(event, fCount == 2 ? 1 : 0);
    } else if (mode == 4) {
      fGun.SetParticlePosition({fCount == 2 ? -25. * mm : 0., -350. * mm, -30. * mm});
      fGun.GeneratePrimaryVertex(event);
    } else if (mode == 5) {
      fGun.SetParticleMomentumDirection({1., 0., 0.});
      fGun.SetParticlePosition({-60. * mm, -350. * mm, 0.});
      fGun.GeneratePrimaryVertex(event);
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

  cupid::TrackAncestry& GetTrackAncestry() { return fEventAction.GetTrackAncestry(); }

  void BeginOfEventAction(const G4Event* event) override {
    fEventAction.BeginOfEventAction(event);
    Check(fEventAction.GetEventRecords().empty() && fEventAction.GetIncidentRecords().empty(),
          "record vectors reset at the start of the event");
    fReference.clear();
    fReferencePDG.clear();
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
    const auto* track = step->GetTrack();
    fReferencePDG.emplace(track->GetTrackID(), track->GetDefinition()->GetPDGEncoding());
    const auto energy = step->GetTotalEnergyDeposit();
    if (energy == 0.) return;
    auto* physical = step->GetPreStepPoint()->GetPhysicalVolume();
    const auto& lmos = fDetector.GetGeometryManager().GetLMOLogicalVolumes();
    // Identify the crystal by its registered logical-volume index, independently of copy number.
    const auto found = std::find(lmos.begin(), lmos.end(), physical->GetLogicalVolume());
    if (found != lmos.end()) {
      fReference[static_cast<int>(std::distance(lmos.begin(), found))] += energy;
      if (step->GetTrack()->GetParentID() > 0) ++secondaryDeposits;
    }
  }

  void EndOfEventAction(const G4Event* event) override {
    fEventAction.EndOfEventAction(event);
    ++checkedEvents;
    Check(!event->IsAborted(), "event completed");
    const auto& eventRows = fEventAction.GetEventRecords();
    const auto& incidentRows = fEventAction.GetIncidentRecords();
    const auto positiveCount = std::count_if(fReference.begin(), fReference.end(),
                                             [](const auto& entry) { return entry.second > 0.; });
    Check(eventRows.size() == static_cast<std::size_t>(positiveCount), "one event row per positive-energy LMO");
    if (fCount == 0) {
      Check(fReference.empty() && eventRows.empty() && incidentRows.empty(), "world-only records empty");
      return;
    }
    auto* collections = event->GetHCofThisEvent();
    const auto id = G4SDManager::GetSDMpointer()->GetCollectionID(
        cupid::LMOSensitiveDetector::CollectionFullName);
    Check(id >= 0 && collections, "collection registered and attached to event");
    if (id < 0 || !collections) return;
    auto* hits = dynamic_cast<cupid::DetectorHitsCollection*>(collections->GetHC(id));
    Check(hits && hits->entries() == static_cast<std::size_t>(fCount), "per-detector hit collection");
    if (!hits) return;

    std::map<G4String, G4double> incidentTotals;
    for (const auto& row : incidentRows) {
      Check(row.eventId == event->GetEventID() && row.trackId > 0 && row.edep > 0., "incident fields");
      Check(fEventAction.GetTrackAncestry().Contains(row.trackId), "root exists in event ancestry");
      Check(fReferencePDG.count(row.trackId) && fReferencePDG.at(row.trackId) == row.pdg,
            "incident PDG matches independently observed root track");
      incidentTotals[row.detectorName] += row.edep;
    }
    G4double total = 0.;
    for (int lmoId = 0; lmoId < fCount; ++lmoId) {
      const G4String name = "LMO" + std::to_string(lmoId);
      const auto found = std::find_if(eventRows.begin(), eventRows.end(), [&](const auto& row) {
        return row.detectorName == name;
      });
      const auto reference = fReference.count(lmoId) ? fReference.at(lmoId) : 0.;
      Check(std::abs(incidentTotals[name] - reference) < 1.e-9 * MeV,
            "sum of incident records matches independent steps without loss or duplication");
      if (reference == 0.) {
        Check(found == eventRows.end(), "zero-energy detector has no event row");
        continue;
      }
      Check(found != eventRows.end(), "positive-energy detector is recorded");
      if (found == eventRows.end()) continue;
      Check(found->eventId == event->GetEventID() && found->edep > 0., "positive event fields");
      Check(std::abs(found->edep - reference) < 1.e-9 * MeV, "event record matches independent steps");
      total += found->edep;
    }
    const auto mode = event->GetEventID() % 6;
    if (mode == 3) {
      Check(total == 0. && eventRows.empty(), "geantino event has no event-energy rows");
      Check(incidentRows.empty(), "zero-deposit incident roots are omitted after grouping");
    } else if (mode < 3) {
      const auto primaries = mode == 2 ? 2 : 1;
      Check(total > 0. && total <= primaries * MeV + 1.e-8 * MeV, "no duplicated event energy");
      for (int primary = 1; primary <= primaries; ++primary) {
        Check(std::any_of(incidentRows.begin(), incidentRows.end(), [&](const auto& row) {
          return row.trackId == primary && row.edep > 0. && row.pdg == 11;
        }), "each in-detector primary has a separate positive root");
      }
    } else if (mode == 5) {
      for (int lmoId = 0; lmoId < fCount; ++lmoId) {
        Check(std::any_of(incidentRows.begin(), incidentRows.end(), [&](const auto& row) {
          return row.trackId == 1 && row.detectorName == "LMO" + std::to_string(lmoId) && row.edep > 0. && row.pdg == 13;
        }), "same external primary records separate deposits in each traversed LMO");
      }
    }
  }

 private:
  const cupid::DetectorConstruction& fDetector;
  int fCount;
  std::map<int, G4double> fReference;
  std::map<int, G4int> fReferencePDG;
  cupid::EventAction fEventAction{{"EventEnergyRecorder", "IncidentEnergyRecorder"}};
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
    SetUserAction(new cupid::TrackingAction(event->GetTrackAncestry()));
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
    run->BeamOn(48);
    run->BeamOn(24);
    Check(checkedEvents == 72, "all events in both runs checked");
    if (count > 0) Check(secondaryDeposits > 0, "test transported depositing secondary particles");
    if (failedChecks != 0) return 1;
    G4cout << "LMO_SD_PASS mode=" << argv[1] << " lmos=" << count
           << " events=" << checkedEvents << G4endl;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
