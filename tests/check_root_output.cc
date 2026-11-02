#include "record/IncidentEnergyRecorder.hh"
#include "record/EventEnergyRecorder.hh"
#include "output/RootOutput.hh"

#include "G4SystemOfUnits.hh"

#include "TDirectory.h"
#include "TFile.h"
#include "TLeaf.h"
#include "TNamed.h"
#include "TKey.h"
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
template<class F> void Reject(F action, const char* message) {
  bool rejected = false;
  try { action(); } catch (const std::exception&) { rejected = true; }
  Require(rejected, message);
}
struct Row { int runId; int eventId; int trackId; double edep; std::string name; int pdg = 0; };
void CheckTree(TFile& file, const char* name, bool incident, const std::vector<Row>& allRows) {
  std::vector<Row> rows;
  for (const auto& row : allRows) if (row.name == name) rows.push_back(row);
  TTree* tree = nullptr;
  file.GetObject(name, tree);
  Require(tree && tree->GetEntries() == static_cast<Long64_t>(rows.size()), "Wrong tree/row count.");
  Require(tree->GetListOfBranches()->GetEntries() == (incident ? 5 : 3), "Wrong branch count.");
  const std::vector<std::string> columns = incident
      ? std::vector<std::string>{"RunID", "EventID", "TrackID", "PDG", "Edep"}
      : std::vector<std::string>{"RunID", "EventID", "Edep"};
  for (std::size_t i = 0; i < columns.size(); ++i)
    Require(columns[i] == tree->GetListOfBranches()->At(i)->GetName(), "Wrong branch order.");
  Require(!tree->GetBranch("DetectorName"), "Redundant detector branch.");
  if (incident) Require(std::string(tree->GetLeaf("PDG")->GetTypeName()) == "Int_t", "Wrong PDG type.");
  Require(std::string(tree->GetLeaf("Edep")->GetTypeName()) == "Double_t", "Wrong energy type.");
  Require(std::string(tree->GetLeaf("EventID")->GetTypeName()) == "Int_t", "Wrong ID type.");
  for (std::size_t i = 0; i < rows.size(); ++i) {
    Require(tree->GetEntry(i) > 0, "Cannot read row.");
    const auto& expected = rows[i];
    Require(tree->GetLeaf("RunID")->GetValue() == expected.runId &&
            tree->GetLeaf("EventID")->GetValue() == expected.eventId &&
            (!incident || (tree->GetLeaf("TrackID")->GetValue() == expected.trackId &&
                           tree->GetLeaf("PDG")->GetValue() == expected.pdg)) &&
            std::abs(tree->GetLeaf("Edep")->GetValue() - expected.edep) <= 1.e-9,
            "Round-trip value mismatch.");
  }
}

std::string FileName(const std::string& base, const char* type) {
  const std::filesystem::path path(base);
  return (path.parent_path() / (path.stem().string() + "_" + type + ".root")).string();
}
void CheckFiles(const std::string& base, const std::vector<Row>& events, const std::vector<Row>& incidents,
                bool eventFile, bool incidentFile) {
  Require(!std::filesystem::exists(base), "Unexpected default output file.");
  for (bool incident : {false, true}) {
    const auto path = FileName(base, incident ? "IncidentEnergy" : "EventEnergy");
    Require(std::filesystem::exists(path) == (incident ? incidentFile : eventFile), "Recorder file selection/emptiness mismatch.");
    if (!std::filesystem::exists(path)) continue;
    TDirectory::TContext context;
    TFile file(path.c_str(), "READ");
    Require(!file.IsZombie(), "Cannot reopen recorder file.");
    Require(file.GetListOfKeys()->GetEntries() == 4, "Unexpected detector keys or metadata cycles.");
    for (const auto* name : {"LMO0", "LMO1", "LD0", "NTD0"})
      CheckTree(file, name, incident, incident ? incidents : events);
    const auto summary = cupid::RootOutput::InspectFile(path);
    Require(summary.Count("EventEnergy") == static_cast<long long>(incident ? 0 : events.size()) &&
            summary.Count("IncidentEnergy") == static_cast<long long>(incident ? incidents.size() : 0), "Split-file summary mismatch.");
  }
}
void CheckIntegrated(const char* base, long long eventRows, long long incidentRows) {
  using Key = std::tuple<int, int, std::string>;
  std::map<Key, double> events, totals;
  std::set<std::tuple<Key, int>> roots;
  for (bool incident : {false, true}) {
    const auto path = FileName(base, incident ? "IncidentEnergy" : "EventEnergy");
    const auto summary = cupid::RootOutput::InspectFile(path);
    Require(summary.Count("EventEnergy") == (incident ? 0 : eventRows) &&
            summary.Count("IncidentEnergy") == (incident ? incidentRows : 0), "Integrated row counts differ.");
    TFile file(path.c_str(), "READ");
    for (const auto* detector : {"LMO0", "LMO1"}) {
      auto* tree = dynamic_cast<TTree*>(file.Get(detector));
      Require(tree && !tree->GetBranch("DetectorName"), "Missing detector tree or redundant branch.");
      for (Long64_t row = 0; row < tree->GetEntries(); ++row) {
        Require(tree->GetEntry(row) > 0, "Cannot read merged row.");
        const int run = tree->GetLeaf("RunID")->GetValue();
        const int event = tree->GetLeaf("EventID")->GetValue();
        const double energy = tree->GetLeaf("Edep")->GetValue();
        Require(run >= 0 && event >= 0 && std::isfinite(energy) && energy > 0., "Invalid merged fields.");
        const Key key{run, event, detector};
        if (!incident) Require(events.emplace(key, energy).second, "Duplicate event row.");
        else {
          const int track = tree->GetLeaf("TrackID")->GetValue();
          Require(track > 0 && roots.emplace(key, track).second, "Duplicate or invalid incident root.");
          Require(tree->GetLeaf("PDG")->GetValue() == 11, "Electron root PDG changed.");
          totals[key] += energy;
        }
      }
    }
  }
  Require(events.size() == totals.size(), "Event/incident keys differ.");
  for (const auto& [key, energy] : events)
    Require(totals.count(key) && std::abs(energy - totals.at(key)) <= 1.e-8, "Event/incident energy mismatch.");
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
    Reject([&] { output.OpenFile(mixed); }, "Missing detector catalog accepted.");
    Reject([&] { output.SetDetectorNames({"LMO0", "LMO0"}); }, "Duplicate detector accepted.");
    Reject([&] { output.SetDetectorNames({"bad/name"}); }, "Unsafe tree name accepted.");
    output.SetDetectorNames({"NTD0", "LMO1", "LMO0", "LD0"});
    TDirectory* previousDirectory = gDirectory;
    Reject([&] { output.Write<cupid::EventEnergyRecord>(0, {}); }, "Writing before open was accepted.");
    Reject([&] { output.OpenFile(""); }, "Empty name was accepted.");
    Reject([&] { output.OpenFile((dir / "missing" / "bad.root").string()); }, "Missing directory was accepted.");
    output.OpenFile(mixed);
    Require(output.IsOpen(), "File not open.");
    Require(gDirectory == previousDirectory, "Open changed caller's current directory.");
    Reject([&] { output.OpenFile((dir / "other.root").string()); }, "Double open accepted.");
    Reject([&] { output.SetDetectorNames({"New0"}); }, "Late detector catalog change accepted.");
    Reject([&] { output.Write<cupid::EventEnergyRecord>(7, {{0, 1., "LMO0"}, {0, 1., "Unknown0"}}); }, "Unknown detector accepted.");
    output.Write<cupid::EventEnergyRecord>(7, {{3, 2.5 * MeV, "LMO0"}, {3, 0., "LMO1"}});
    output.Write<cupid::IncidentEnergyRecord>(7, {{3, 9, 22, 1.5 * MeV, "LMO0"}, {3, 11, -11, 1. * MeV, "LMO0"},
                                    {3, 12, 0, 0., "LMO1"}});
    output.Write<cupid::EventEnergyRecord>(8, {{3, 0.25 * keV, "LD0"}});
    output.Write<cupid::IncidentEnergyRecord>(8, {{3, 1, 1000902320, 0.25 * keV, "LD0"}});
    Reject([&] { output.Write<cupid::EventEnergyRecord>(0, {{4, 1., "NTD0"}, {4, -1., "NTD0"}}); }, "Invalid batch accepted.");
    Reject([&] { output.Write<cupid::EventEnergyRecord>(-1, {}); }, "Invalid RunID accepted.");
    Reject([&] { output.Write<cupid::EventEnergyRecord>(0, {{-1, 1., "LMO0"}}); }, "Invalid EventID accepted.");
    Reject([&] { output.Write<cupid::EventEnergyRecord>(0, {{0, 1., ""}}); }, "Empty detector name accepted.");
    Reject([&] { output.Write<cupid::EventEnergyRecord>(0, {{0, std::numeric_limits<double>::infinity(), "LMO0"}}); }, "Infinite energy accepted.");
    Reject([&] { output.Write<cupid::IncidentEnergyRecord>(0, {{0, 0, 22, 1., "LMO0"}}); }, "Invalid TrackID accepted.");
    output.CloseFile();
    Require(!output.IsOpen(), "File stayed open.");
    output.CloseFile();
    Require(gDirectory == previousDirectory, "Writing changed caller's current directory.");
    Reject([&] { output.Write<cupid::IncidentEnergyRecord>(0, {}); }, "Writing after close accepted.");
    Reject([&] { output.OpenFile(mixed); }, "Existing file was overwritten.");
    CheckFiles(mixed, {{7, 3, 0, 2500., "LMO0"}, {7, 3, 0, 0., "LMO1"}, {8, 3, 0, 0.25, "LD0"}},
                     {{7, 3, 9, 1500., "LMO0", 22}, {7, 3, 11, 1000., "LMO0", -11},
                      {7, 3, 12, 0., "LMO1"}, {8, 3, 1, 0.25, "LD0", 1000902320}}, true, true);
    // Native CloseFile resets data; schemas persist across runs.
    const auto repeated = (dir / "repeated.root").string();
    output.OpenFile(repeated);
    output.Write<cupid::EventEnergyRecord>(9, {{2, 1. * keV, "NTD0"}});
    output.CloseFile();
    CheckFiles(repeated, {{9, 2, 0, 1., "NTD0"}}, {}, true, false);
    const auto empty = (dir / "empty.root").string();
    output.OpenFile(empty);
    output.CloseFile();
    CheckFiles(empty, {}, {}, false, false);
    }
    // A new adapter may book a different schema after its predecessor is destroyed.
    {
      cupid::RootOutput output;
      output.Book(std::vector<cupid::IncidentEnergyRecord>{});
      output.SetDetectorNames({"NTD0"});
      output.OpenFile((dir / "large.root").string());
      Reject([&] { output.Book(std::vector<cupid::EventEnergyRecord>{}); }, "Late booking accepted.");
      Reject([&] { output.Write<cupid::EventEnergyRecord>(0, {}); }, "Unbooked write accepted.");
      for (int event = 0; event < 100005; ++event)
        output.Write<cupid::IncidentEnergyRecord>(3, {{event, event + 1, event % 2 ? -11 : 1000902320, event * keV, "NTD0"}});
      output.CloseFile();
      TFile file((dir / "large_IncidentEnergy.root").c_str(), "READ");
      std::vector<Row> rows;
      for (int event = 0; event < 100005; ++event) rows.push_back({3, event, event + 1, double(event), "NTD0", event % 2 ? -11 : 1000902320});
      CheckTree(file, "NTD0", true, rows);
      Require(!std::filesystem::exists(dir / "large_EventEnergy.root"), "Unselected recorder file created.");
      Require(!file.Get("EventEnergy"), "Unselected schema created.");
    }
    {
      cupid::RootOutput output;
      output.Book(std::vector<cupid::EventEnergyRecord>{});
      output.SetDetectorNames({});
      const auto empty = (dir / "no-detectors.root").string();
      output.OpenFile(empty);
      output.Write<cupid::EventEnergyRecord>(0, {});
      output.CloseFile();
      Require(output.GetFileNames(empty).empty() && !std::filesystem::exists(empty), "Zero-detector output created a file.");
    }
    std::cout << "ROOT output checks passed; files: " << dir << '\n';
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
