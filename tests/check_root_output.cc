#include "output/RootOutput.hh"

#include "G4SystemOfUnits.hh"

#include "TDirectory.h"
#include "TFile.h"
#include "TLeaf.h"
#include "TLeafC.h"
#include "TNamed.h"
#include "TTree.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <tuple>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void Require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
const char* DetectorName(TTree* tree) {
  const auto* leaf = dynamic_cast<TLeafC*>(tree->GetLeaf("DetectorName"));
  Require(leaf != nullptr, "Expected native Geant4 string leaf.");
  return leaf->GetValueString();
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
  for (std::size_t i = 0; i < rows.size(); ++i) {
    Require(tree->GetEntry(i) > 0, "Cannot read row.");
    const auto& expected = rows[i];
    Require(tree->GetLeaf("RunID")->GetValue() == expected.runId &&
            tree->GetLeaf("EventID")->GetValue() == expected.eventId &&
            (!incident || tree->GetLeaf("TrackID")->GetValue() == expected.trackId) &&
            std::string(DetectorName(tree)) == expected.name &&
            std::abs(tree->GetLeaf("Edep")->GetValue() - expected.edep) <= 1.e-9,
            "Round-trip value mismatch.");
  }
}

void CheckIntegrated(const char* name, long long eventRows, long long incidentRows) {
  const auto summary = cupid::RootOutput::InspectFile(name);
  Require(summary.eventRows == eventRows && summary.incidentRows == incidentRows, "Integrated row counts differ.");
  TFile file(name, "READ");
  using Key = std::tuple<int, int, std::string>;
  std::map<Key, double> events, totals;
  std::set<std::tuple<Key, int>> roots;
  for (const auto* table : {"EventEnergy", "IncidentEnergy"}) {
    auto* tree = dynamic_cast<TTree*>(file.Get(table));
    Require(tree != nullptr, "Missing selected ntuple.");
    for (Long64_t row = 0; row < tree->GetEntries(); ++row) {
      Require(tree->GetEntry(row) > 0, "Cannot read merged row.");
      const int run = tree->GetLeaf("RunID")->GetValue();
      const int event = tree->GetLeaf("EventID")->GetValue();
      const std::string detector = DetectorName(tree);
      const double energy = tree->GetLeaf("Edep")->GetValue();
      Require(run >= 0 && event >= 0 && std::isfinite(energy) && energy >= 0., "Invalid merged fields.");
      const Key key{run, event, detector};
      if (std::string(table) == "EventEnergy") {
        Require(energy > 0. && events.emplace(key, energy).second, "Duplicate or zero event row.");
      } else {
        const int track = tree->GetLeaf("TrackID")->GetValue();
        Require(track > 0 && roots.emplace(key, track).second, "Duplicate or invalid incident root.");
        totals[key] += energy;
      }
    }
  }
  for (const auto& [key, energy] : events)
    Require(std::abs(energy - totals[key]) <= 1.e-8, "Event/incident energy mismatch.");
  for (const auto& [key, energy] : totals)
    Require(std::abs(energy - events[key]) <= 1.e-8, "Incident/event energy mismatch.");
}
}

int main(int argc, char** argv) {
  try {
    if (argc == 5 && std::string(argv[1]) == "--inspect") {
      CheckIntegrated(argv[2], std::stoll(argv[3]), std::stoll(argv[4]));
      return 0;
    }
    Require(argc == 2, "Expected test output directory.");
    const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto dir = std::filesystem::path(argv[1]) / std::to_string(stamp);
    std::filesystem::create_directories(dir);
    const auto mixed = (dir / "mixed.root").string();
    {
    cupid::RootOutput output;
    Reject([] { cupid::RootOutput duplicate; }, "Second analysis adapter in same thread accepted.");
    output.Book(std::vector<cupid::EventEnergyRecord>{});
    output.Book(std::vector<cupid::IncidentEnergyRecord>{});
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
      CheckTree(file, "EventEnergy", false, {{7, 3, 0, 2500., "LMO0"}, {7, 3, 0, 0., "LMO1"}, {8, 3, 0, 0.25, "LD0"}});
      CheckTree(file, "IncidentEnergy", true, {{7, 3, 9, 1500., "LMO0"}, {7, 3, 11, 1000., "LMO0"},
                                               {7, 3, 12, 0., "LMO1"}, {8, 3, 1, 0.25, "LD0"}});
    }
    // Native CloseFile resets data; schemas persist across runs.
    const auto repeated = (dir / "repeated.root").string();
    output.OpenFile(repeated);
    output.WriteEventRecords(9, {{2, 1. * keV, "NTD0"}});
    output.CloseFile();
    {
      TFile file(repeated.c_str(), "READ");
      CheckTree(file, "EventEnergy", false, {{9, 2, 0, 1., "NTD0"}});
      CheckTree(file, "IncidentEnergy", true, {});
    }
    const auto empty = (dir / "empty.root").string();
    output.OpenFile(empty);
    output.CloseFile();
    Require(!std::filesystem::exists(empty), "Native empty-file removal did not occur.");
    }
    // A new adapter may book a different schema after its predecessor is destroyed.
    {
      cupid::RootOutput output;
      output.Book(std::vector<cupid::IncidentEnergyRecord>{});
      output.OpenFile((dir / "large.root").string());
      Reject([&] { output.Book(std::vector<cupid::EventEnergyRecord>{}); }, "Late booking accepted.");
      Reject([&] { output.WriteEventRecords(0, {}); }, "Unbooked write accepted.");
      for (int event = 0; event < 100005; ++event)
        output.WriteIncidentRecords(3, {{event, event + 1, event * keV, "NTD0"}});
      output.CloseFile();
      TFile file((dir / "large.root").c_str(), "READ");
      std::vector<Row> rows;
      for (int event = 0; event < 100005; ++event) rows.push_back({3, event, event + 1, double(event), "NTD0"});
      CheckTree(file, "IncidentEnergy", true, rows);
      Require(!file.Get("EventEnergy"), "Unselected schema created.");
    }
    std::cout << "ROOT output checks passed; files: " << dir << '\n';
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
