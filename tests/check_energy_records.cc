#include "DetectorHit.hh"
#include "EventAction.hh"
#include "record/EventEnergyRecorder.hh"
#include "record/IncidentEnergyRecorder.hh"
#include "record/TrackAncestry.hh"

#include "G4DecayPhysics.hh"
#include "G4Event.hh"
#include "G4RunManager.hh"
#include "G4VModularPhysicsList.hh"
#include "G4HCofThisEvent.hh"

#include <cmath>
#include <iostream>
#include <limits>
#include <map>
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
using Key = std::pair<G4String, G4int>;
}

int main() {
  try {
    G4RunManager run;
    auto* physics = new G4VModularPhysicsList;
    physics->RegisterPhysics(new G4DecayPhysics(0));
    run.SetUserInitialization(physics);
    cupid::TrackAncestry ancestry;
    ancestry.RegisterTrack(1, 0, "World", 22);
    ancestry.RegisterTrack(2, 1, "LMO0", 11);
    ancestry.RegisterTrack(3, 2, "LMO0", 11);
    ancestry.RegisterTrack(4, 0, "World", -13);
    ancestry.RegisterTrack(5, 4, "LMO0", 11);
    ancestry.RegisterTrack(6, 2, "LMO1", 11);
    ancestry.RegisterTrack(7, 2, "World", 11);
    ancestry.RegisterTrack(8, 0, "LMO0", 1000902320);
    ancestry.RegisterTrack(9, 8, "LMO0", 11);
    ancestry.RegisterTrack(10, 0, "World", 0);
    ancestry.RegisterTrack(11, 2, "World", 11);
    ancestry.RegisterTrack(12, 6, "LMO0", 11);

    cupid::DetectorHitsCollection hits("TestSD", "Edep");
    auto* lmo0 = new cupid::DetectorHit("LMO0");
    hits.insert(lmo0);
    for (const auto& [id, energy] : std::map<int, double>{
        {1, 0.}, {2, 10.}, {3, 20.}, {4, 5.}, {5, 15.}, {7, 3.},
        {8, 11.}, {9, 12.}, {10, 0.}, {11, 2.}, {12, 7.}})
      lmo0->AddEnergyDeposit(id, energy);
    lmo0->AddEnergyDeposit(11, 4.);  // Same track re-enters the same detector.
    auto* lmo1 = new cupid::DetectorHit("LMO1");
    lmo1->AddEnergyDeposit(2, 4.);
    lmo1->AddEnergyDeposit(6, 6.);
    hits.insert(lmo1);
    auto* ld = new cupid::DetectorHit("LD0");
    ld->AddEnergyDeposit(2, 1.);
    hits.insert(ld);
    hits.insert(new cupid::DetectorHit("NTD0"));

    const cupid::EventEnergyRecorder eventRecorder;
    const cupid::IncidentEnergyRecorder incidentRecorder;
    const auto eventRows = eventRecorder.Build(42, hits);
    Require(eventRows.size() == 3, "omit zero-deposit detectors in event rows");
    std::map<G4String, double> totals;
    for (const auto& row : eventRows) {
      Require(row.eventId == 42, "event field");
      totals[row.detectorName] = row.edep;
    }
    Require(totals == std::map<G4String, double>{{"LMO0", 89.}, {"LMO1", 10.}, {"LD0", 1.}},
            "event totals and generic detector names");
    std::map<Key, double> actual;
    std::map<G4String, double> incidentTotals;
    for (const auto& row : incidentRecorder.Build(42, hits, ancestry)) {
      Require(row.eventId == 42 && row.edep > 0., "positive incident event field");
      const auto expectedPDG = row.trackId == 1 ? 22 : row.trackId == 4 ? -13 : row.trackId == 8 ? 1000902320 : 11;
      Require(row.pdg == expectedPDG, "PDG belongs to incident root, not depositing descendant");
      Require(actual.emplace(Key{row.detectorName, row.trackId}, row.edep).second, "unique root rows");
      incidentTotals[row.detectorName] += row.edep;
    }
    const std::map<Key, double> expected{
        {{"LMO0", 1}, 30.}, {{"LMO0", 4}, 20.}, {{"LMO0", 7}, 3.},
        {{"LMO0", 8}, 23.}, {{"LMO0", 11}, 6.},
        {{"LMO0", 12}, 7.}, {{"LMO1", 2}, 10.}, {{"LD0", 2}, 1.}};
    Require(actual == expected, "roots: secondaries, cross-detector entry, re-entry, internal primary, zero ancestors");
    for (const auto& [name, total] : totals)
      Require(incidentTotals[name] == total, "incident/event equality");

    // Both an untouched detector and zero-energy visits produce no event rows.
    cupid::DetectorHitsCollection zeroHits("ZeroSD", "Edep");
    zeroHits.insert(new cupid::DetectorHit("LMO0"));
    auto* zeroVisit = new cupid::DetectorHit("LMO1");
    zeroVisit->AddEnergyDeposit(1, 0.);
    zeroVisit->AddEnergyDeposit(2, 0.);
    zeroHits.insert(zeroVisit);
    Require(eventRecorder.Build(43, zeroHits).empty(), "all-zero event produces no rows");
    Require(incidentRecorder.Build(43, zeroHits, ancestry).empty(), "all-zero roots produce no incident rows");
    const G4double tinyEnergy = 1.e-20;
    zeroVisit->AddEnergyDeposit(2, tinyEnergy);
    const auto tinyRows = eventRecorder.Build(44, zeroHits);
    Require(tinyRows.size() == 1 && tinyRows[0].detectorName == "LMO1" &&
            tinyRows[0].eventId == 44 && tinyRows[0].edep == tinyEnergy,
            "retain arbitrarily small positive deposits without an energy threshold");

    const auto tinyIncidents = incidentRecorder.Build(44, zeroHits, ancestry);
    Require(tinyIncidents.size() == 1 && tinyIncidents[0].trackId == 2 &&
            tinyIncidents[0].pdg == 11 && tinyIncidents[0].edep == tinyEnergy,
            "retain positive incident totals without a threshold");
    cupid::DetectorHitsCollection splitHits("SplitSD", "Edep");
    auto* split0 = new cupid::DetectorHit("LMO0");
    auto* split1 = new cupid::DetectorHit("LMO1");
    split0->AddEnergyDeposit(10, 0.);
    split1->AddEnergyDeposit(10, 3.);
    splitHits.insert(split0);
    splitHits.insert(split1);
    const auto splitRows = incidentRecorder.Build(45, splitHits, ancestry);
    Require(splitRows.size() == 1 && splitRows[0].detectorName == "LMO1" &&
            splitRows[0].trackId == 10 && splitRows[0].pdg == 0 && splitRows[0].edep == 3.,
            "filter per detector, retaining exact zero PDG encoding");

    // Reverse IDs exercise a long first traversal rather than cached one-link paths.
    cupid::TrackAncestry longAncestry;
    cupid::DetectorHitsCollection longHits("LongSD", "Edep");
    auto* deep = new cupid::DetectorHit("LMO0");
    longHits.insert(deep);
    for (int id = 1; id <= 20000; ++id) {
      longAncestry.RegisterTrack(id, id == 20000 ? 0 : id + 1, id == 20000 ? "World" : "LMO0", id == 20000 ? 1000902320 : 11);
      deep->AddEnergyDeposit(id, id == 1 ? 5. : 0.);
    }
    const auto longRows = incidentRecorder.Build(0, longHits, longAncestry);
    Require(longRows.size() == 1 && longRows[0].trackId == 20000 && longRows[0].edep == 5. && longRows[0].pdg == 1000902320,
            "long genealogy without recursion or ordering assumptions");

    cupid::TrackAncestry cycle;
    cycle.RegisterTrack(1, 2, "LMO0", 11);
    cycle.RegisterTrack(2, 1, "LMO0", 11);
    cupid::DetectorHitsCollection badHits("BadSD", "Edep");
    auto* bad = new cupid::DetectorHit("LMO0");
    bad->AddEnergyDeposit(1, 1.);
    bad->AddEnergyDeposit(2, 0.);
    badHits.insert(bad);
    Reject([&] { incidentRecorder.Build(0, badHits, cycle); }, "cycle rejected");
    cupid::TrackAncestry missing;
    Reject([&] { incidentRecorder.Build(0, badHits, missing); }, "missing track rejected");
    missing.RegisterTrack(1, 99, "LMO0", 11);
    Reject([&] { incidentRecorder.Build(0, badHits, missing); }, "missing parent rejected");
    Reject([&] { ancestry.RegisterTrack(1, 0, "LMO1", 11); }, "conflicting ancestry rejected");
    ancestry.RegisterTrack(1, 0, "World", 22);
    Reject([&] { ancestry.RegisterTrack(1, 0, "World", 11); }, "conflicting PDG rejected");
    Reject([&] { lmo0->AddEnergyDeposit(0, 1.); }, "invalid track rejected");
    Reject([&] { lmo0->AddEnergyDeposit(1, -1.); }, "negative energy rejected");
    Reject([&] { lmo0->AddEnergyDeposit(1, std::numeric_limits<double>::quiet_NaN()); }, "NaN rejected");

    // Exercise the production EventAction, including holes in the collection ID table.
    cupid::EventAction action({"EventEnergyRecorder", "IncidentEnergyRecorder"});
    G4Event event(7);
    auto* collections = new G4HCofThisEvent(3);
    auto* actionHits = new cupid::DetectorHitsCollection("LMOSD", "Edep");
    auto* actionHit = new cupid::DetectorHit("LMO0");
    actionHit->AddEnergyDeposit(1, 2.);
    actionHits->insert(actionHit);
    collections->AddHitsCollection(2, actionHits);
    event.SetHCofThisEvent(collections);
    action.BeginOfEventAction(&event);
    action.GetTrackAncestry().RegisterTrack(1, 0, "LMO0", 11);
    action.EndOfEventAction(&event);
    Require(action.GetRecords<cupid::EventEnergyRecord>().size() == 1 && action.GetRecords<cupid::IncidentEnergyRecord>().size() == 1,
            "Action handles non-contiguous collection IDs");
    const auto copied = action.GetRecords<cupid::EventEnergyRecord>();
    G4Event empty(8);
    action.BeginOfEventAction(&empty);
    action.EndOfEventAction(&empty);
    Require(action.GetRecords<cupid::EventEnergyRecord>().empty() && action.GetRecords<cupid::IncidentEnergyRecord>().empty() &&
            !action.GetTrackAncestry().Contains(1), "next event clears ancestry and records");
    Require(copied[0].eventId == 7 && copied[0].edep == 2., "record values survive independent copying");
    // The default EventAction enables both recorders and requires ancestry.
    cupid::EventAction defaultAction;
    Require(defaultAction.RequiresTrackAncestry(), "default selects both recorders");
    defaultAction.BeginOfEventAction(&event);
    defaultAction.GetTrackAncestry().RegisterTrack(1, 0, "LMO0", 11);
    defaultAction.EndOfEventAction(&event);
    Require(defaultAction.GetRecords<cupid::EventEnergyRecord>().size() == 1 && defaultAction.GetRecords<cupid::IncidentEnergyRecord>().size() == 1,
            "default invokes both recorders");
    for (const bool eventEnabled : {false, true}) {
      for (const bool incidentEnabled : {false, true}) {
        std::vector<G4String> names;
        if (eventEnabled) names.push_back("EventEnergyRecorder");
        if (incidentEnabled) names.push_back("IncidentEnergyRecorder");
        cupid::EventAction selected(names);
        Require(selected.RequiresTrackAncestry() == incidentEnabled, "selected ancestry requirement");
        selected.BeginOfEventAction(&event);
        if (incidentEnabled) selected.GetTrackAncestry().RegisterTrack(1, 0, "LMO0", 11);
        selected.EndOfEventAction(&event);
        Require(selected.GetRecords<cupid::EventEnergyRecord>().size() == (eventEnabled ? 1u : 0u), "independent event selection");
        Require(selected.GetRecords<cupid::IncidentEnergyRecord>().size() == (incidentEnabled ? 1u : 0u), "independent incident selection");
        if (eventEnabled) Require(selected.GetRecords<cupid::EventEnergyRecord>()[0].edep == 2., "selected event energy");
        if (incidentEnabled) Require(selected.GetRecords<cupid::IncidentEnergyRecord>()[0].edep == 2., "selected incident energy");
        selected.BeginOfEventAction(&empty);
        selected.EndOfEventAction(&empty);
        Require(selected.GetRecords<cupid::EventEnergyRecord>().empty() && selected.GetRecords<cupid::IncidentEnergyRecord>().empty(),
                "selection does not retain previous event results");
      }
    }
    action.BeginOfEventAction(&event);
    event.SetEventAborted();
    action.EndOfEventAction(&event);
    Require(action.GetRecords<cupid::EventEnergyRecord>().empty() && action.GetRecords<cupid::IncidentEnergyRecord>().empty(), "aborted event excluded");
    std::cout << "All energy record checks passed.\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
