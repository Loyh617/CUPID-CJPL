#include "record/IncidentEnergyRecorder.hh"

#include <map>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace cupid {

std::vector<IncidentEnergyRecord> IncidentEnergyRecorder::Build(
    G4int eventId, const DetectorHitsCollection& hits, const TrackAncestry& ancestry) const {
  std::vector<IncidentEnergyRecord> records;
  for (const auto* hit : *hits.GetVector()) {
    const auto& name = hit->GetDetectorName();
    const auto& deposits = hit->GetTrackDeposits();
    std::unordered_map<G4int, G4int> roots;
    std::map<G4int, G4double> totals;

    for (const auto& [trackId, edep] : deposits) {
      auto current = trackId;
      G4int root = 0;
      std::vector<G4int> path;
      std::unordered_set<G4int> visiting;
      while (true) {
        const auto cached = roots.find(current);
        if (cached != roots.end()) {
          root = cached->second;
          break;
        }
        if (!visiting.insert(current).second)
          throw std::runtime_error("Cycle in track ancestry for detector " + name);
        path.push_back(current);
        const auto& origin = ancestry.Get(current);
        if (origin.parentId > 0) ancestry.Get(origin.parentId);

        // A new entry starts a new root for this detector. A primary born here is also a root.
        // A parent with no visit here cannot establish an incident branch in this detector.
        if (origin.parentId == 0 || origin.birthVolume != name ||
            deposits.count(origin.parentId) == 0) {
          root = current;
          break;
        }
        current = origin.parentId;
      }
      // Cache the entire path, avoiding recursion and repeated traversal of long decay chains.
      for (const auto id : path) roots[id] = root;
      totals[root] += edep;
    }
    for (const auto& [trackId, edep] : totals)
      records.push_back({eventId, trackId, edep, name});
  }
  return records;
}

}  // namespace cupid
