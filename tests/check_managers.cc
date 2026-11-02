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

void CheckTree(TFile& file, const char* treeName, bool incident) {
  auto* tree = dynamic_cast<TTree*>(file.Get(treeName));
  Require(tree && !tree->GetBranch("DetectorName"), "detector tree missing or redundant branch");
  const bool lmo = std::string(treeName) == "LMO0";
  const bool empty = std::string(treeName) == "LMO1";
  Require(tree->GetEntries() == (empty ? 0 : 1), "incorrect manager row count");
  if (empty) return;
  Require(tree->GetEntry(0) > 0, "cannot read managed output row");
  Require(tree->GetLeaf("RunID")->GetValue() == 3 && tree->GetLeaf("EventID")->GetValue() == 7,
          "record IDs changed in manager dispatch");
  Require(std::abs(tree->GetLeaf("Edep")->GetValue() - (lmo ? 2500. : 1.)) < 1.e-9, "managed energy changed");
  if (incident) {
    Require(tree->GetLeaf("TrackID")->GetValue() == (lmo ? 1 : 2), "managed root changed");
    Require(tree->GetLeaf("PDG")->GetValue() == (lmo ? 22 : 11), "managed root PDG changed");
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
    ancestry.RegisterTrack(1, 0, "World", 22);
    ancestry.RegisterTrack(2, 1, "LMO0", 11);
    ancestry.RegisterTrack(3, 0, "World", 11);

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
      Require(records.GetRecords<cupid::IncidentEnergyRecord>().size() == (incidents ? 2u : 0u), "incident selection failed");

      cupid::OutputManager output("RootOutput");
      Require(output.GetName() == "RootOutput" && output.GetFileExtension() == ".root", "wrong output backend");
      Reject([&] { output.Write(3, std::vector<cupid::EventEnergyRecord>{}); }, "write before open accepted");
      const auto fileName = (directory / ("selection" + std::to_string(index) + output.GetFileExtension())).string();
      output.SetDetectorNames({"LMO0", "LMO1", "LD0"});
      for (const auto& batch : records.GetBatches())
        std::visit([&](const auto& rows) { output.Book(rows); }, batch);
      output.OpenFile(fileName);
      Require(output.IsOpen(), "manager did not open output");
      Reject([&] { output.InspectFile(fileName); }, "inspection with open writer accepted");
      for (const auto& batch : records.GetBatches())
        std::visit([&](const auto& rows) { output.Write(3, rows); }, batch);
      output.CloseFile();
      Require(!output.IsOpen(), "manager did not close output");
      Require(!std::filesystem::exists(fileName), "Unexpected default file.");
      const auto files = output.GetFileNames(fileName);
      Require(files.size() == static_cast<std::size_t>(events + incidents), "Wrong selected output files.");
      for (const auto& path : files) {
        paths.push_back(path);
        const bool incident = path.find("_IncidentEnergy.root") != std::string::npos;
        const auto summary = output.InspectFile(path);
        Require(summary.eventRows == (incident ? 0 : 2) && summary.incidentRows == (incident ? 2 : 0), "Managed summary mismatch.");
        TFile file(path.c_str(), "READ");
        Require(!file.IsZombie(), "Invalid managed ROOT file.");
        for (const auto* name : {"LMO0", "LMO1", "LD0"}) CheckTree(file, name, incident);
      }

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

    for (const auto& path : paths) std::filesystem::remove(path);
    std::filesystem::remove(directory);
    std::cout << "All record/output manager checks passed.\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
