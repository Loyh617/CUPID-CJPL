#include "output/RootOutput.hh"

#include "G4SystemOfUnits.hh"

#include "TDirectory.h"
#include "TFile.h"
#include "TLeaf.h"
#include "TNamed.h"
#include "TTree.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <future>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
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
struct Row { int runId; int eventId; int trackId; double edep; std::string name; };
void CheckTree(TFile& file, const char* name, bool incident, const std::vector<Row>& rows) {
  TTree* tree = nullptr;
  file.GetObject(name, tree);
  Require(tree && tree->GetEntries() == static_cast<Long64_t>(rows.size()), "Wrong tree/row count.");
  Require(tree->GetListOfBranches()->GetEntries() == (incident ? 5 : 4), "Wrong branch count.");
  Require(std::string(tree->GetLeaf("Edep")->GetTypeName()) == "Double_t", "Wrong energy type.");
  Require(std::string(tree->GetLeaf("EventID")->GetTypeName()) == "Int_t", "Wrong ID type.");
  Row value{};
  std::string* nameValue = &value.name;
  Require(tree->SetBranchAddress("RunID", &value.runId) >= 0, "RunID binding failed.");
  Require(tree->SetBranchAddress("EventID", &value.eventId) >= 0, "EventID binding failed.");
  Require(tree->SetBranchAddress("Edep", &value.edep) >= 0, "Edep binding failed.");
  Require(tree->SetBranchAddress("DetectorName", &nameValue) >= 0, "DetectorName binding failed.");
  if (incident) Require(tree->SetBranchAddress("TrackID", &value.trackId) >= 0, "TrackID binding failed.");
  for (std::size_t i = 0; i < rows.size(); ++i) {
    Require(tree->GetEntry(i) > 0, "Cannot read row.");
    const auto& expected = rows[i];
    Require(value.runId == expected.runId && value.eventId == expected.eventId &&
            (!incident || value.trackId == expected.trackId) && value.name == expected.name &&
            std::abs(value.edep - expected.edep) <= 1.e-9, "Round-trip value mismatch.");
  }
  tree->ResetBranchAddresses();
}
void CheckMetadata(TFile& file) {
  TNamed* unit = nullptr;
  TNamed* schema = nullptr;
  file.GetObject("EnergyUnit", unit);
  file.GetObject("SchemaVersion", schema);
  Require(unit && std::string(unit->GetTitle()) == "keV", "Missing energy unit.");
  Require(schema && std::string(schema->GetTitle()) == "1", "Missing schema version.");
}
}

int main(int argc, char** argv) {
  try {
    if (argc == 5 && std::string(argv[1]) == "--inspect") {
      const auto summary = cupid::RootOutput::InspectFile(argv[2]);
      Require(summary.eventRows == std::stoll(argv[3]) && summary.incidentRows == std::stoll(argv[4]),
              "Unexpected integrated output row counts.");
      return 0;
    }
    Require(argc == 2, "Expected test output directory.");
    const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto dir = std::filesystem::path(argv[1]) / std::to_string(stamp);
    std::filesystem::create_directories(dir);
    const auto mixed = (dir / "mixed.root").string();
    cupid::RootOutput output;
    TDirectory* previousDirectory = gDirectory;
    Reject([&] { output.WriteEventRecords(0, {}); }, "Writing before open was accepted.");
    Reject([&] { output.OpenFile(""); }, "Empty name was accepted.");
    Reject([&] { output.OpenFile((dir / "missing" / "bad.root").string()); }, "Missing directory was accepted.");
    output.OpenFile(mixed);
    Require(output.IsOpen(), "File not open.");
    Require(gDirectory == previousDirectory, "Open changed caller's current directory.");
    Reject([&] { output.OpenFile((dir / "other.root").string()); }, "Double open accepted.");
    output.WriteEventRecords(7, {{3, 2.5 * MeV, "LMO0"}, {3, 0., "LMO1"}});
    output.WriteIncidentRecords(7, {{3, 9, 1.5 * MeV, "LMO0"}, {3, 11, 1. * MeV, "LMO0"},
                                    {3, 12, 0., "LMO1"}});
    output.WriteEventRecords(8, {{3, 0.25 * keV, "LD0"}});
    output.WriteIncidentRecords(8, {{3, 1, 0.25 * keV, "LD0"}});
    Reject([&] { output.WriteEventRecords(0, {{4, 1., "NTD0"}, {4, -1., "NTD0"}}); }, "Invalid batch accepted.");
    Reject([&] { output.WriteEventRecords(-1, {}); }, "Invalid RunID accepted.");
    Reject([&] { output.WriteEventRecords(0, {{-1, 1., "LMO0"}}); }, "Invalid EventID accepted.");
    Reject([&] { output.WriteEventRecords(0, {{0, 1., ""}}); }, "Empty detector name accepted.");
    Reject([&] { output.WriteEventRecords(0, {{0, std::numeric_limits<double>::infinity(), "LMO0"}}); }, "Infinite energy accepted.");
    Reject([&] { output.WriteIncidentRecords(0, {{0, 0, 1., "LMO0"}}); }, "Invalid TrackID accepted.");
    output.CloseFile();
    Require(!output.IsOpen(), "File stayed open.");
    output.CloseFile();
    Require(gDirectory == previousDirectory, "Writing changed caller's current directory.");
    Reject([&] { output.WriteIncidentRecords(0, {}); }, "Writing after close accepted.");
    Reject([&] { output.OpenFile(mixed); }, "Existing file was overwritten.");
    {
      TDirectory::TContext context;
      TFile file(mixed.c_str(), "READ");
      Require(!file.IsZombie(), "Cannot reopen ROOT file.");
      CheckMetadata(file);
      CheckTree(file, "EventEnergy", false, {{7, 3, 0, 2500., "LMO0"}, {7, 3, 0, 0., "LMO1"}, {8, 3, 0, 0.25, "LD0"}});
      CheckTree(file, "IncidentEnergy", true, {{7, 3, 9, 1500., "LMO0"}, {7, 3, 11, 1000., "LMO0"},
                                               {7, 3, 12, 0., "LMO1"}, {8, 3, 1, 0.25, "LD0"}});
    }
    // Reuse the same writer without retaining trees or rows from the previous file.
    const auto empty = (dir / "empty.root").string();
    output.OpenFile(empty);
    output.WriteEventRecords(0, {});
    output.CloseFile();
    {
      TDirectory::TContext context;
      TFile file(empty.c_str(), "READ");
      CheckTree(file, "EventEnergy", false, {});
      Require(!file.Get("IncidentEnergy"), "Unrequested tree was created.");
    }
    const auto metadataOnly = (dir / "metadata-only.root").string();
    output.OpenFile(metadataOnly);
    output.CloseFile();
    {
      TDirectory::TContext context;
      TFile file(metadataOnly.c_str(), "READ");
      CheckMetadata(file);
      Require(!file.Get("EventEnergy") && !file.Get("IncidentEnergy"), "Unexpected empty trees.");
    }
    // Exercise flush/autosave boundaries and destructor finalization with independent workers.
    std::vector<std::future<void>> workers;
    for (int worker = 0; worker < 2; ++worker) {
      workers.push_back(std::async(std::launch::async, [&, worker] {
        cupid::RootOutput local;
        local.OpenFile((dir / ("worker" + std::to_string(worker) + ".root")).string());
        for (int event = 0; event < 100005; ++event)
          local.WriteIncidentRecords(worker, {{event, 1, event * keV, "NTD0"}});
        if (worker == 0) local.CloseFile();
      }));
    }
    for (auto& worker : workers) worker.get();
    for (int worker = 0; worker < 2; ++worker) {
      TDirectory::TContext context;
      TFile file((dir / ("worker" + std::to_string(worker) + ".root")).c_str(), "READ");
      Require(!file.IsZombie(), "Cannot read worker file.");
      std::vector<Row> rows;
      for (int event = 0; event < 100005; ++event) rows.push_back({worker, event, 1, double(event), "NTD0"});
      CheckTree(file, "IncidentEnergy", true, rows);
      Require(!file.Get("EventEnergy"), "Unrequested event tree was created.");
      CheckMetadata(file);
    }
    const auto merged = (dir / "merged.root").string();
    const auto summary = cupid::RootOutput::MergeFiles(merged, {metadataOnly, mixed, empty});
    Require(summary.eventRows == 3 && summary.incidentRows == 4, "Merged row count mismatch.");
    Require(std::filesystem::exists(mixed) && std::filesystem::exists(empty), "Merger deleted inputs.");
    Require(!std::filesystem::exists(merged + ".partial"), "Successful merge left a partial file.");
    {
      TDirectory::TContext context;
      TFile file(merged.c_str(), "READ");
      CheckMetadata(file);
      CheckTree(file, "EventEnergy", false, {{7, 3, 0, 2500., "LMO0"}, {7, 3, 0, 0., "LMO1"}, {8, 3, 0, 0.25, "LD0"}});
      CheckTree(file, "IncidentEnergy", true, {{7, 3, 9, 1500., "LMO0"}, {7, 3, 11, 1000., "LMO0"},
                                               {7, 3, 12, 0., "LMO1"}, {8, 3, 1, 0.25, "LD0"}});
    }
    Reject([&] { cupid::RootOutput::MergeFiles(merged, {mixed}); }, "Merge overwrote existing output.");
    const auto invalid = (dir / "invalid.root").string();
    Reject([&] { cupid::RootOutput::MergeFiles(invalid, {}); }, "Merge accepted no inputs.");
    Reject([&] { cupid::RootOutput::MergeFiles(invalid, {mixed, mixed}); }, "Merge accepted duplicate inputs.");
    Reject([&] { cupid::RootOutput::MergeFiles(invalid, {mixed, (dir / "missing.root").string()}); },
           "Merge accepted missing input.");
    const auto corrupt = (dir / "corrupt.root").string();
    { std::ofstream file(corrupt); file << "not a ROOT file"; }
    Reject([&] { cupid::RootOutput::MergeFiles(invalid, {mixed, corrupt}); }, "Merge accepted invalid input.");
    Require(!std::filesystem::exists(invalid) && std::filesystem::exists(mixed), "Failed merge lost input data.");
    const auto zeroSummary = cupid::RootOutput::MergeFiles((dir / "zero.root").string(), {metadataOnly, empty});
    Require(zeroSummary.eventRows == 0 && zeroSummary.incidentRows == 0, "Empty merge failed.");
    std::cout << "ROOT output checks passed; files: " << dir << '\n';
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
