#include "record/EventEnergyRecorder.hh"

namespace cupid {

std::vector<EventEnergyRecord> EventEnergyRecorder::Build(
    G4int eventId, const DetectorHitsCollection& hits) const {
  std::vector<EventEnergyRecord> records;
  records.reserve(hits.entries());
  for (const auto* hit : *hits.GetVector()) {
    G4double total = 0.;
    for (const auto& deposit : hit->GetTrackDeposits()) total += deposit.second;
    records.push_back({eventId, total, hit->GetDetectorName()});
  }
  return records;
}

}  // namespace cupid
