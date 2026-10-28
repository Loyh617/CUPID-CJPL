#include "DetectorHit.hh"
#include "output/OutputManager.hh"
#include "record/RecordManager.hh"

#include "G4DecayPhysics.hh"
#include "G4HCofThisEvent.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4VModularPhysicsList.hh"

#include "TFile.h"
#include "TLeaf.h"
#include "TTree.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace {
void Require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
template<class F> void Reject(F action, const char* message) {
  bool rejected = false;
  try { action(); } catch (const std::exception&) { rejected = true; }
  Require(rejected, message);
}

void CheckTree(TFile& file, const char* treeName, bool expected, bool incident) {
  TTree* tree = nullptr;
  file.GetObject(treeName, tree);
  Require((tree != nullptr) == expected, "unselected tree created or selected tree missing");
  if (!tree) return;
  Require(tree->GetEntries() == (incident ? 3 : 2), "incorrect manager row count");
  std::string* detector = nullptr;
  tree->SetBranchAddress("DetectorName", &detector);
  std::map<std::pair<std::string, int>, double> actual;
  for (Long64_t row = 0; row < tree->GetEntries(); ++row) {
    Require(tree->GetEntry(row) > 0, "cannot read managed output row");
    Require(tree->GetLeaf("RunID")->GetValue() == 3 && tree->GetLeaf("EventID")->GetValue() == 7,
            "record IDs changed in manager dispatch");
    const auto track = incident ? static_cast<int>(tree->GetLeaf("TrackID")->GetValue()) : 0;
    Require(actual.emplace(std::make_pair(*detector, track), tree->GetLeaf("Edep")->GetValue()).second,
            "duplicate managed row");
  }
  tree->ResetBranchAddresses();
  delete detector;
  const std::map<std::pair<std::string, int>, double> wanted = incident
      ? std::map<std::pair<std::string, int>, double>{{{"LMO0", 1}, 2500.}, {{"LMO0", 3}, 0.}, {{"LD0", 2}, 1.}}
      : std::map<std::pair<std::string, int>, double>{{{"LMO0", 0}, 2500.}, {{"LD0", 0}, 1.}};
  Require(actual.size() == wanted.size(), "managed row key count");
  for (const auto& [key, energy] : wanted) {
    const auto found = actual.find(key);
    Require(found != actual.end() && std::abs(found->second - energy) < 1.e-9,
            "managed record identity, ancestry or unit conversion changed");
  }
}
}

int main(int argc, char** argv) {
  try {
    Require(argc == 2, "expected a test output directory");
    G4RunManager run;
    auto* physics = new G4VModularPhysicsList;
    physics->RegisterPhysics(new G4DecayPhysics(0));
    run.SetUserInitialization(physics);
    cupid::TrackAncestry ancestry;
    ancestry.RegisterTrack(1, 0, "World");
    ancestry.RegisterTrack(2, 1, "LMO0");
    ancestry.RegisterTrack(3, 0, "World");

    G4HCofThisEvent collections(4);
    auto* first = new cupid::DetectorHitsCollection("LMOSD", "Edep");
    auto* lmo = new cupid::DetectorHit("LMO0");
    lmo->AddEnergyDeposit(1, 2. * MeV);
    lmo->AddEnergyDeposit(2, 0.5 * MeV);
    lmo->AddEnergyDeposit(3, 0.);
    first->insert(lmo);
    first->insert(new cupid::DetectorHit("LMO1"));
    collections.AddHitsCollection(1, first);
    auto* second = new cupid::DetectorHitsCollection("LDSD", "Edep");
    auto* ld = new cupid::DetectorHit("LD0");
    ld->AddEnergyDeposit(2, 1. * keV);
    second->insert(ld);
    collections.AddHitsCollection(3, second);

    Reject([] { cupid::RecordManager manager({"UnknownRecorder"}); }, "unknown recorder accepted");
    Reject([] { cupid::RecordManager manager({""}); }, "empty recorder name accepted");
    Reject([] { cupid::RecordManager manager({"EventEnergyRecorder", "EventEnergyRecorder"}); },
           "duplicate recorder accepted");
    Reject([] { cupid::RecordManager manager({"eventenergyrecorder"}); }, "recorder case mismatch accepted");
    Reject([] { cupid::OutputManager manager("UnknownOutput"); }, "unknown output accepted");
    Reject([] { cupid::OutputManager manager(""); }, "empty output name accepted");
    Reject([] { cupid::OutputManager manager("rootoutput"); }, "output case mismatch accepted");

    const auto directory = std::filesystem::path(argv[1]) /
        ("job_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directories(directory), "test directory already exists");
    std::vector<std::string> paths;
    const std::vector<std::vector<G4String>> selections{
        {}, {"EventEnergyRecorder"}, {"IncidentEnergyRecorder"},
        {"EventEnergyRecorder", "IncidentEnergyRecorder"},
        {"IncidentEnergyRecorder", "EventEnergyRecorder"}};
    for (std::size_t index = 0; index < selections.size(); ++index) {
      cupid::RecordManager records(selections[index]);
      const bool events = records.IsEnabled("EventEnergyRecorder");
      const bool incidents = records.IsEnabled("IncidentEnergyRecorder");
      Require(records.GetNames() == selections[index], "selection order changed");
      Require(records.RequiresTrackAncestry() == incidents, "wrong ancestry requirement");
      const cupid::TrackAncestry emptyAncestry;
      records.Build(7, collections, incidents ? ancestry : emptyAncestry);
      Require(records.GetRecords<cupid::EventEnergyRecord>().size() == (events ? 2u : 0u), "event selection failed");
      Require(records.GetRecords<cupid::IncidentEnergyRecord>().size() == (incidents ? 3u : 0u), "incident selection failed");

      cupid::OutputManager output("RootOutput");
      Require(output.GetName() == "RootOutput" && output.GetFileExtension() == ".root", "wrong output backend");
      Reject([&] { output.Write(3, std::vector<cupid::EventEnergyRecord>{}); }, "write before open accepted");
      const auto fileName = (directory / ("selection" + std::to_string(index) + output.GetFileExtension())).string();
      paths.push_back(fileName);
      output.OpenFile(fileName);
      Require(output.IsOpen(), "manager did not open output");
      Reject([&] { output.InspectFile(fileName); }, "inspection with open writer accepted");
      Reject([&] { output.MergeFiles((directory / "invalid.root").string(), {fileName}); },
             "merge with open writer accepted");
      for (const auto& batch : records.GetBatches())
        std::visit([&](const auto& rows) { output.Write(3, rows); }, batch);
      output.CloseFile();
      Require(!output.IsOpen(), "manager did not close output");
      const auto summary = output.InspectFile(fileName);
      Require(summary.eventRows == (events ? 2 : 0) && summary.incidentRows == (incidents ? 3 : 0),
              "managed file summary differs");
      TFile file(fileName.c_str(), "READ");
      Require(!file.IsZombie(), "managed ROOT file invalid");
      CheckTree(file, "EventEnergy", events, false);
      CheckTree(file, "IncidentEnergy", incidents, true);
      file.Close();

      G4HCofThisEvent noCollections(0);
      records.Build(8, noCollections, emptyAncestry);
      for (const auto& batch : records.GetBatches())
        std::visit([](const auto& rows) { Require(rows.empty(), "previous event data retained"); }, batch);
      Require(records.GetBatches().size() == selections[index].size(), "clear lost selected result types");
    }

    G4HCofThisEvent duplicateCollections(3);
    for (int id : {0, 2}) {
      auto* hits = new cupid::DetectorHitsCollection("DuplicateSD", "Edep");
      hits->insert(new cupid::DetectorHit("LMO0"));
      duplicateCollections.AddHitsCollection(id, hits);
    }
    cupid::RecordManager records({"EventEnergyRecorder"});
    Reject([&] { records.Build(7, duplicateCollections, ancestry); }, "duplicate detector accepted");
    Reject([&] { records.Build(-1, collections, ancestry); }, "negative EventID accepted");

    cupid::OutputManager merger("RootOutput");
    const auto merged = (directory / "merged.root").string();
    const auto summary = merger.MergeFiles(merged, {paths[0], paths[1], paths[2]});
    paths.push_back(merged);
    Require(summary.eventRows == 2 && summary.incidentRows == 3, "managed merge lost rows");
    TFile mergedFile(merged.c_str(), "READ");
    CheckTree(mergedFile, "EventEnergy", true, false);
    CheckTree(mergedFile, "IncidentEnergy", true, true);
    mergedFile.Close();
    for (const auto& path : paths) Require(std::filesystem::exists(path), "merge deleted an input");
    for (const auto& path : paths) std::filesystem::remove(path);
    std::filesystem::remove(directory);
    std::cout << "All record/output manager checks passed.\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
