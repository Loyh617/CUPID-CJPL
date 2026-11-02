/// \file include/record/EventEnergyRecorder.hh
/// \brief Declaration: Produce positive per-event, per-detector total energy records.
///
/// EventAction calls Build(eventId, hits) after transport. The returned
/// EventEnergyRecord vector contains totals > 0 in Geant4 internal units.
/// The recorder performs no file I/O; EventAction selects the output module.

#pragma once

#include "DetectorHit.hh"
#include "record/RecordSchema.hh"

#include "G4SystemOfUnits.hh"
#include "globals.hh"

#include <cmath>
#include <stdexcept>
#include <vector>

namespace cupid {

// Energies are unweighted and remain in Geant4 internal units.
struct EventEnergyRecord {
  G4int eventId;
  G4double edep;
  G4String detectorName;
};

template<> struct RecordTraits<EventEnergyRecord> {
  static const RecordDescription<EventEnergyRecord>& Get() {
    using R = EventEnergyRecord;
    static const auto description = Describe<R>(
        "EventEnergy", DetectorKey(&R::detectorName),
        Column("EventID", &R::eventId),
        Column("Edep", &R::edep, keV, "keV"))
        .WithValidation([](const R& record) {
          if (record.eventId < 0 || !std::isfinite(record.edep) || record.edep < 0.)
            throw std::invalid_argument("EventEnergyRecord: invalid record.");
        });
    return description;
  }
};

class EventEnergyRecorder {
 public:
  // Emit only detectors with positive total energy; apply no additional energy threshold.
  std::vector<EventEnergyRecord> Build(G4int eventId, const DetectorHitsCollection& hits) const;
};

}  // namespace cupid
