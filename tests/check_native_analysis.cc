// Exercise production RunAction/OutputManager with synthetic, distinct row values.
// This verifies native Geant4 MT booking/merging, not detector physics.
#include "RunAction.hh"

#include "G4Box.hh"
#include "G4DecayPhysics.hh"
#include "G4Event.hh"
#include "G4Geantino.hh"
#include "G4LogicalVolume.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4ParticleGun.hh"
#include "G4RunManager.hh"
#include "G4RunManagerFactory.hh"
#include "G4SystemOfUnits.hh"
#include "G4UserEventAction.hh"
#include "G4VModularPhysicsList.hh"
#include "G4VUserActionInitialization.hh"
#include "G4VUserDetectorConstruction.hh"
#include "G4VUserPrimaryGeneratorAction.hh"

#include "TFile.h"
#include "TLeaf.h"
#include "TLeafC.h"
#include "TTree.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void Require(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
int TestPDG(int id) {
  const int codes[] = {22, -11, 1000902320, 0};
  return codes[id % 4];
}
class Geometry final : public G4VUserDetectorConstruction {
 public:
  G4VPhysicalVolume* Construct() override {
    auto* logical = new G4LogicalVolume(new G4Box("World", m, m, m),
        G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic"), "World");
    return new G4PVPlacement(nullptr, {}, logical, "World", nullptr, false, 0);
  }
};
class Generator final : public G4VUserPrimaryGeneratorAction {
 public:
  Generator() { fGun.SetParticleDefinition(G4Geantino::Definition()); fGun.SetParticleEnergy(MeV); }
  void GeneratePrimaries(G4Event* event) override { fGun.GeneratePrimaryVertex(event); }
 private:
  G4ParticleGun fGun{1};
};
class Event final : public G4UserEventAction {
 public:
  Event(cupid::RunAction* run, bool events, bool incidents) : fRun(run), fEvents(events), fIncidents(incidents) {}
  void EndOfEventAction(const G4Event* event) override {
    const auto run = fRun->GetRunId();
    if (run == 2) return;
    const auto id = event->GetEventID();
    const auto name = "LMO" + std::to_string(id % 2);
    const auto energy = (id + 0.25) * keV;
    if (fEvents) fRun->GetOutput().Write(run, std::vector<cupid::EventEnergyRecord>{{id, energy, name}});
    if (fIncidents) fRun->GetOutput().Write(run, std::vector<cupid::IncidentEnergyRecord>{{id, id + 1000, TestPDG(id), energy, name}});
  }
 private:
  cupid::RunAction* fRun;
  bool fEvents, fIncidents;
};
class Actions final : public G4VUserActionInitialization {
 public:
  Actions(std::string directory, bool events, bool incidents) : fDirectory(directory), fEvents(events), fIncidents(incidents) {
    // Reverse registration order to detect hard-coded ntuple-ID assumptions.
    if (incidents) fNames.push_back("IncidentEnergyRecorder");
    if (events) fNames.push_back("EventEnergyRecorder");
  }
  void BuildForMaster() const override { SetUserAction(new cupid::RunAction(fDirectory, fNames)); }
  void Build() const override {
    auto* run = new cupid::RunAction(fDirectory, fNames);
    SetUserAction(run);
    SetUserAction(new Generator);
    SetUserAction(new Event(run, fEvents, fIncidents));
  }
 private:
  std::string fDirectory;
  std::vector<G4String> fNames;
  bool fEvents, fIncidents;
};
void Check(TFile& file, const char* name, bool selected, int run, int count, bool incident) {
  auto* tree = dynamic_cast<TTree*>(file.Get(name));
  Require((tree != nullptr) == selected, "Selected schema mismatch");
  if (!selected) return;
  Require(tree->GetEntries() == count, "Native merge lost or duplicated rows");
  auto* detector = dynamic_cast<TLeafC*>(tree->GetLeaf("DetectorName"));
  Require(detector != nullptr, "Missing native string leaf");
  const std::vector<std::string> columns = incident
      ? std::vector<std::string>{"RunID", "EventID", "TrackID", "PDG", "Edep", "DetectorName"}
      : std::vector<std::string>{"RunID", "EventID", "Edep", "DetectorName"};
  Require(tree->GetListOfBranches()->GetEntries() == static_cast<int>(columns.size()), "Wrong column count");
  for (std::size_t i = 0; i < columns.size(); ++i)
    Require(columns[i] == tree->GetListOfBranches()->At(i)->GetName(), "Merged column order changed");
  std::set<int> ids;
  for (int row = 0; row < count; ++row) {
    Require(tree->GetEntry(row) > 0, "Cannot read native row");
    const int id = tree->GetLeaf("EventID")->GetValue();
    Require(id >= 0 && id < count && ids.insert(id).second, "Event IDs not preserved");
    Require(tree->GetLeaf("RunID")->GetValue() == run, "Run IDs not preserved");
    Require(std::abs(tree->GetLeaf("Edep")->GetValue() - (id + 0.25)) < 1.e-10, "Column association or energy changed");
    Require(std::string(detector->GetValueString()) == "LMO" + std::to_string(id % 2), "Detector association changed");
    if (incident) {
      Require(tree->GetLeaf("TrackID")->GetValue() == id + 1000, "Track association changed");
      Require(tree->GetLeaf("PDG")->GetValue() == TestPDG(id), "PDG association changed");
    }
  }
}
}
int main(int argc, char** argv) {
  try {
    Require(argc == 3, "Expected test directory and selection");
    const std::string mode = argv[2];
    const bool events = mode == "both" || mode == "event";
    const bool incidents = mode == "both" || mode == "incident";
    Require(events || incidents || mode == "none", "Unknown selection");
    const auto directory = std::filesystem::path(argv[1]) /
        (mode + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directories(directory), "Test directory already exists");
    {
      std::unique_ptr<G4RunManager> run(G4RunManagerFactory::CreateRunManager(G4RunManagerType::MTOnly));
      run->SetNumberOfThreads(4);
      run->SetUserInitialization(new Geometry);
      auto* physics = new G4VModularPhysicsList;
      physics->RegisterPhysics(new G4DecayPhysics(0));
      run->SetUserInitialization(physics);
      run->SetUserInitialization(new Actions(directory.string(), events, incidents));
      run->Initialize();
      run->BeamOn(29);
      run->BeamOn(1);
      run->BeamOn(7);
    }
    for (int run = 0; run < 3; ++run) {
      const auto path = directory / ("run" + std::to_string(run) + ".root");
      const bool hasFile = run < 2 && (events || incidents);
      Require(std::filesystem::exists(path) == hasFile, "Empty-file policy mismatch");
      if (!hasFile) continue;
      TFile file(path.c_str(), "READ");
      Require(!file.IsZombie(), "Invalid native file");
      Check(file, "EventEnergy", events, run, run == 0 ? 29 : 1, false);
      Check(file, "IncidentEnergy", incidents, run, run == 0 ? 29 : 1, true);
      file.Close();
      std::filesystem::remove(path);
    }
    Require(std::filesystem::is_empty(directory), "Unexpected worker/extra files");
    std::filesystem::remove(directory);
    std::cout << "Native MT selection, row association and run lifecycle passed: " << mode << '\n';
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
