// Exercise production RunAction/OutputManager with synthetic, distinct row values.
// This verifies native Geant4 MT booking/merging, not detector physics.
#include "record/IncidentEnergyRecorder.hh"
#include "record/EventEnergyRecorder.hh"
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
    if (fEvents && run != 4) fRun->GetOutput().Write(run, std::vector<cupid::EventEnergyRecord>{{id, energy, name}});
    if (fIncidents && run != 3) fRun->GetOutput().Write(run, std::vector<cupid::IncidentEnergyRecord>{{id, id + 1000, TestPDG(id), energy, name}});
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
  void BuildForMaster() const override { SetUserAction(new cupid::RunAction(fDirectory, fNames, "RootOutput", [] { return std::vector<G4String>{"LMO1", "LD0", "LMO0"}; })); }
  void Build() const override {
    auto* run = new cupid::RunAction(fDirectory, fNames, "RootOutput", [] { return std::vector<G4String>{"LMO1", "LD0", "LMO0"}; });
    SetUserAction(run);
    SetUserAction(new Generator);
    SetUserAction(new Event(run, fEvents, fIncidents));
  }
 private:
  std::string fDirectory;
  std::vector<G4String> fNames;
  bool fEvents, fIncidents;
};
void Check(TFile& file, int run, int count, bool incident) {
  Require(file.GetListOfKeys()->GetEntries() == 3, "Unexpected tree keys or cycles");
  std::set<int> ids;
  for (const auto* name : {"LMO0", "LMO1", "LD0"}) {
    auto* tree = dynamic_cast<TTree*>(file.Get(name));
    Require(tree && !tree->GetBranch("DetectorName"), "Detector tree schema mismatch");
    const int expected = std::string(name) == "LMO0" ? (count + 1) / 2 : std::string(name) == "LMO1" ? count / 2 : 0;
    Require(tree->GetEntries() == expected, "Native merge lost/duplicated/misrouted rows");
    const std::vector<std::string> columns = incident
        ? std::vector<std::string>{"RunID", "EventID", "TrackID", "PDG", "Edep"}
        : std::vector<std::string>{"RunID", "EventID", "Edep"};
    Require(tree->GetListOfBranches()->GetEntries() == static_cast<int>(columns.size()), "Wrong column count");
    for (std::size_t i = 0; i < columns.size(); ++i)
      Require(columns[i] == tree->GetListOfBranches()->At(i)->GetName(), "Merged column order changed");
    for (int row = 0; row < expected; ++row) {
      Require(tree->GetEntry(row) > 0, "Cannot read native row");
      const int id = tree->GetLeaf("EventID")->GetValue();
      Require(id >= 0 && id < count && ids.insert(id).second, "Event IDs not preserved");
      Require(tree->GetLeaf("RunID")->GetValue() == run, "Run IDs not preserved");
      Require(std::abs(tree->GetLeaf("Edep")->GetValue() - (id + 0.25)) < 1.e-10, "Column association or energy changed");
      Require(std::string(name) == "LMO" + std::to_string(id % 2), "Detector association changed");
      if (incident) {
        Require(tree->GetLeaf("TrackID")->GetValue() == id + 1000, "Track association changed");
        Require(tree->GetLeaf("PDG")->GetValue() == TestPDG(id), "PDG association changed");
      }
    }
  }
  Require(static_cast<int>(ids.size()) == count, "Incomplete merged event coverage");
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
      run->BeamOn(5);
      run->BeamOn(5);
    }
    for (int run = 0; run < 5; ++run) {
      Require(!std::filesystem::exists(directory / ("run" + std::to_string(run) + ".root")), "Unexpected default file");
      for (bool incident : {false, true}) {
        const auto path = directory / ("run" + std::to_string(run) + (incident ? "_IncidentEnergy.root" : "_EventEnergy.root"));
        const bool hasFile = run != 2 && (incident ? incidents && run != 3 : events && run != 4);
        Require(std::filesystem::exists(path) == hasFile, "Empty/selected file policy mismatch");
        if (!hasFile) continue;
        TFile file(path.c_str(), "READ");
        Require(!file.IsZombie(), "Invalid native file");
        Check(file, run, run == 0 ? 29 : run == 1 ? 1 : 5, incident);
        file.Close();
        std::filesystem::remove(path);
      }
    }
    Require(std::filesystem::is_empty(directory), "Unexpected worker/extra files");
    std::filesystem::remove(directory);
    std::cout << "Native MT selection, row association and run lifecycle passed: " << mode << '\n';
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
