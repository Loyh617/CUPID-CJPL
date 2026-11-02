/// \file src/record/RecordManager.cc
/// \brief Run selected recorders and collect their typed event results.
///
/// Validate recorder names once, then call each selected recorder explicitly.
/// Build() checks detector-name uniqueness across all collections before adding
/// records; Clear() removes the preceding event results.

#include "record/RecordManager.hh"

#include "G4HCofThisEvent.hh"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace cupid {
RecordManager::RecordManager(const std::vector<G4String>& names) {
  for (const auto& name : names) {
    if (IsEnabled(name)) throw std::invalid_argument("Duplicate recorder selection: " + name);
    if (name == "EventEnergyRecorder") fEvents = true;
    else if (name == "IncidentEnergyRecorder") fIncidents = true;
    else throw std::invalid_argument("Unknown recorder: " + name);
    fNames.push_back(name);
  }
}

bool RecordManager::IsEnabled(const G4String& name) const {
  return std::find(fNames.begin(), fNames.end(), name) != fNames.end();
}

void RecordManager::Clear() {
  fEventRecords.clear();
  fIncidentRecords.clear();
}

void RecordManager::Build(G4int eventId, G4HCofThisEvent& collections,
                          const TrackAncestry& ancestry) {
  Clear();
  if (eventId < 0) throw std::invalid_argument("RecordManager: negative EventID.");
  if (Empty()) return;
  std::vector<const DetectorHitsCollection*> detectorCollections;
  std::set<G4String> detectorNames;
  for (std::size_t id = 0; id < collections.GetCapacity(); ++id) {
    const auto* hits = dynamic_cast<const DetectorHitsCollection*>(collections.GetHC(id));
    if (!hits) continue;
    for (const auto* hit : *hits->GetVector())
      if (!detectorNames.insert(hit->GetDetectorName()).second)
        throw std::runtime_error("Duplicate detector name across event collections.");
    detectorCollections.push_back(hits);
  }
  for (const auto* hits : detectorCollections) {
    if (fEvents) {
      const auto records = EventEnergyRecorder{}.Build(eventId, *hits);
      fEventRecords.insert(fEventRecords.end(), records.begin(), records.end());
    }
    if (fIncidents) {
      const auto records = IncidentEnergyRecorder{}.Build(eventId, *hits, ancestry);
      fIncidentRecords.insert(fIncidentRecords.end(), records.begin(), records.end());
    }
  }
}

}  // namespace cupid
