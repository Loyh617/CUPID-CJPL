// A test-only record exercises new fields without adding output/action overloads.
// Synthetic values test serialization and native merging, not detector physics.
#include "output/OutputManager.hh"
#include "record/RecordBatch.hh"

#include "G4Box.hh"
#include "G4DecayPhysics.hh"
#include "G4Event.hh"
#include "G4Geantino.hh"
#include "G4LogicalVolume.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4ParticleGun.hh"
#include "G4Run.hh"
#include "G4RunManager.hh"
#include "G4RunManagerFactory.hh"
#include "G4SystemOfUnits.hh"
#include "G4UserEventAction.hh"
#include "G4UserRunAction.hh"
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
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace cupid {
struct TimingRecord {
  G4String sensor;
  G4double time;
  G4String label;
  G4int sample;
  G4float amplitude;
};
template<> struct RecordTraits<TimingRecord> {
  static const RecordDescription<TimingRecord>& Get() {
    using R = TimingRecord;
    static const auto description = Describe<R>("Timing", DetectorKey(&R::sensor),
        Column("Sample", &R::sample), Column("Label", &R::label),
        Column("Amplitude", &R::amplitude), Column("Time", &R::time, us, "us"));
    return description;
  }
};
struct AlternateTimingRecord { G4String detector; G4int code; };
template<> struct RecordTraits<AlternateTimingRecord> {
  static const RecordDescription<AlternateTimingRecord>& Get() {
    using R = AlternateTimingRecord;
    static const auto description = Describe<R>("Timing", DetectorKey(&R::detector), Column("Code", &R::code));
    return description;
  }
};
}

namespace {
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class F> void Reject(F action, const char* message) {
  bool rejected = false;
  try { action(); } catch (const std::exception&) { rejected = true; }
  Require(rejected, message);
}
using cupid::TimingRecord;
using cupid::RecordTraits;
void CheckSchema() {
  const auto& description = RecordTraits<TimingRecord>::Get();
  auto schema = description.schema;
  schema.Validate();
  schema.name = "../bad";
  Reject([&] { schema.Validate(); }, "Unsafe dataset name accepted");
  schema = description.schema;
  schema.columns[0].name = "RunID";
  Reject([&] { schema.Validate(); }, "Reserved column accepted");
  schema.columns[0].name = "Label";
  Reject([&] { schema.Validate(); }, "Duplicate column accepted");
  schema = description.schema;
  schema.columns.back().scale = 0.;
  Reject([&] { schema.Validate(); }, "Zero unit scale accepted");
  schema.columns.back().scale = std::numeric_limits<double>::infinity();
  Reject([&] { schema.Validate(); }, "Infinite unit scale accepted");
  schema = description.schema;
  schema.columns[0].unit = "us";
  Reject([&] { schema.Validate(); }, "Integer units accepted");
  schema = description.schema;
  schema.columns.clear();
  Reject([&] { schema.Validate(); }, "Empty schema accepted");
  schema = description.schema;
  schema.columns[0].type = static_cast<cupid::ColumnType>(99);
  Reject([&] { schema.Validate(); }, "Unsupported column type accepted");
  auto batch = cupid::RecordBatch::Create<TimingRecord>();
  Require(batch.Get<TimingRecord>() && !batch.Get<G4int>(), "Batch type checking failed");
  batch.Get<TimingRecord>()->push_back({"Sensor0", 7.5 * us, "signal", -7, 0.25f});
  auto moved = std::move(batch);
  auto view = moved.View();
  Require(view.Size() == 1 && view.DetectorName(0) == "Sensor0" &&
          std::get<G4int>(view.Value(0, 0)) == -7 &&
          std::get<G4String>(view.Value(0, 1)) == "signal" &&
          std::get<G4float>(view.Value(0, 2)) == 0.25f &&
          std::get<G4double>(view.Value(0, 3)) == 7.5, "Description readback or conversion failed");
  moved.Clear();
  Require(moved.View().Size() == 0 && moved.View().Schema() == description.schema,
          "Empty batch lost its schema");
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
class Run final : public G4UserRunAction {
 public:
  explicit Run(const std::string& directory) : fDirectory(directory), fOutput("RootOutput") {
    const auto& schema = RecordTraits<TimingRecord>::Get().schema;
    fOutput.Book(schema);
    fOutput.Book(schema);
    auto conflicting = schema;
    conflicting.columns.back().scale = ns;
    Reject([&] { fOutput.Book(conflicting); }, "Conflicting schema accepted");
    fOutput.SetDetectorNames({"Sensor1", "Empty0", "Sensor0"});
  }
  void BeginOfRunAction(const G4Run* run) override {
    fRun = run->GetRunID();
    fOutput.OpenFile(fDirectory + "/run" + std::to_string(fRun) + ".root");
  }
  void EndOfRunAction(const G4Run*) override { fOutput.CloseFile(); }
  int GetId() const { return fRun; }
  cupid::OutputManager& Output() { return fOutput; }
 private:
  std::string fDirectory;
  cupid::OutputManager fOutput;
  int fRun = -1;
};
class Event final : public G4UserEventAction {
 public:
  explicit Event(Run* run) : fRun(run), fBatch(cupid::RecordBatch::Create<TimingRecord>()) {}
  void EndOfEventAction(const G4Event* event) override {
    fBatch.Clear();
    Reject([&] { fRun->Output().Write(0, std::vector<cupid::AlternateTimingRecord>{}); },
           "Write accepted a different schema with the same dataset name");
    if (fRun->GetId() == 1) return;
    const auto id = event->GetEventID();
    auto& rows = *fBatch.Get<TimingRecord>();
    rows.push_back({"Sensor" + std::to_string(id % 2), (id + 0.5) * us,
                   "sample " + std::to_string(id), id - 20, static_cast<float>(id + 0.25)});
    auto invalid = rows;
    invalid.push_back({"Sensor0", std::numeric_limits<double>::infinity(), "invalid", 0, 0.});
    Reject([&] { fRun->Output().Write(fRun->GetId(), invalid); }, "Invalid double batch accepted");
    invalid.back().time = 0.;
    invalid.back().amplitude = std::numeric_limits<float>::infinity();
    Reject([&] { fRun->Output().Write(fRun->GetId(), invalid); }, "Invalid float batch accepted");
    fRun->Output().Write(fRun->GetId(), fBatch.View());
  }
 private:
  Run* fRun;
  cupid::RecordBatch fBatch;
};
class Actions final : public G4VUserActionInitialization {
 public:
  explicit Actions(std::string directory) : fDirectory(std::move(directory)) {}
  void BuildForMaster() const override { SetUserAction(new Run(fDirectory)); }
  void Build() const override {
    auto* run = new Run(fDirectory);
    SetUserAction(run);
    SetUserAction(new Generator);
    SetUserAction(new Event(run));
  }
 private:
  std::string fDirectory;
};

void CheckFile(const std::string& path, int run, int count) {
  cupid::OutputManager output("RootOutput");
  Require(output.InspectFile(path).Count("Timing") == count, "Generic inspection row mismatch");
  TFile file(path.c_str(), "READ");
  Require(!file.IsZombie() && file.GetListOfKeys()->GetEntries() == 3, "Unexpected generic output layout");
  std::set<int> seen;
  for (const auto* detector : {"Sensor0", "Sensor1", "Empty0"}) {
    auto* tree = dynamic_cast<TTree*>(file.Get(detector));
    Require(tree && !tree->GetBranch("sensor") && !tree->GetBranch("DetectorName"), "Detector key leaked into branches");
    Require(std::string(tree->GetTitle()) == "Timing; Time in us; CUPID schema 4", "Generic unit metadata mismatch");
    const std::vector<std::string> names{"RunID", "Sample", "Label", "Amplitude", "Time"};
    const std::vector<std::string> types{"Int_t", "Int_t", "Char_t", "Float_t", "Double_t"};
    Require(tree->GetListOfBranches()->GetEntries() == 5, "Generic column count mismatch");
    for (std::size_t column = 0; column < names.size(); ++column) {
      Require(names[column] == tree->GetListOfBranches()->At(column)->GetName(), "Generic column order mismatch");
      auto* leaf = tree->GetLeaf(names[column].c_str());
      Require(leaf && types[column] == leaf->GetTypeName(), "Generic column type mismatch");
    }
    for (Long64_t row = 0; row < tree->GetEntries(); ++row) {
      Require(tree->GetEntry(row) > 0, "Cannot read generic row");
      const int id = tree->GetLeaf("Sample")->GetValue() + 20;
      auto* label = dynamic_cast<TLeafC*>(tree->GetLeaf("Label"));
      Require(id >= 0 && id < count && seen.insert(id).second && label &&
          std::string(label->GetValueString()) == "sample " + std::to_string(id) &&
          tree->GetLeaf("RunID")->GetValue() == run &&
          tree->GetLeaf("Amplitude")->GetValue() == id + 0.25 &&
          tree->GetLeaf("Time")->GetValue() == id + 0.5 &&
          std::string(detector) == "Sensor" + std::to_string(id % 2), "Generic row association/conversion failed");
    }
  }
  Require(static_cast<int>(seen.size()) == count, "Generic merge lost rows");
}
}

int main(int argc, char** argv) {
  try {
    Require(argc == 3, "Expected output directory and serial/mt mode");
    CheckSchema();
    const std::string mode = argv[2];
    Require(mode == "serial" || mode == "mt", "Invalid mode");
    const auto directory = std::filesystem::path(argv[1]) /
        (mode + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directories(directory), "Test output already exists");
    {
      std::unique_ptr<G4RunManager> run(G4RunManagerFactory::CreateRunManager(
          mode == "mt" ? G4RunManagerType::MTOnly : G4RunManagerType::SerialOnly));
      if (mode == "mt") run->SetNumberOfThreads(4);
      run->SetUserInitialization(new Geometry);
      auto* physics = new G4VModularPhysicsList;
      physics->RegisterPhysics(new G4DecayPhysics(0));
      run->SetUserInitialization(physics);
      run->SetUserInitialization(new Actions(directory.string()));
      run->Initialize();
      run->BeamOn(37);
      run->BeamOn(3);
      run->BeamOn(1);
    }
    CheckFile((directory / "run0_Timing.root").string(), 0, 37);
    CheckFile((directory / "run2_Timing.root").string(), 2, 1);
    std::filesystem::remove(directory / "run0_Timing.root");
    std::filesystem::remove(directory / "run2_Timing.root");
    Require(std::filesystem::is_empty(directory), "Unexpected empty/default/worker files");
    std::filesystem::remove(directory);
    std::cout << "Generic schema, scalar types, unit conversion and batch routing passed: " << mode << '\n';
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
