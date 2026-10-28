/// \file src/record/EventEnergyRecorder.cc
/// \brief Implementation: Produce positive per-event, per-detector total energy records.
///
/// EventAction calls Build(eventId, hits) after transport. The returned
/// EventEnergyRecord vector contains totals > 0 in Geant4 internal units.
/// The recorder performs no file I/O; EventAction selects the output module.

#include "record/EventEnergyRecorder.hh"

namespace cupid {

std::vector<EventEnergyRecord> EventEnergyRecorder::Build(
    G4int eventId, const DetectorHitsCollection& hits) const {
  std::vector<EventEnergyRecord> records;
  records.reserve(hits.entries());
  for (const auto* hit : *hits.GetVector()) {
    G4double total = 0.;
    for (const auto& deposit : hit->GetTrackDeposits()) total += deposit.second;
    if (total > 0.) records.push_back({eventId, total, hit->GetDetectorName()});
  }
  return records;
}

}  // namespace cupid
