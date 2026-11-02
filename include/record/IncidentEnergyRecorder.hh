/// \file include/record/IncidentEnergyRecorder.hh
/// \brief Declaration: Group detector energy deposits by their incident ancestor track.
///
/// EventAction calls Build(eventId, hits, ancestry) after tracking completes.
/// Grouping is detector-local and follows parent IDs and birth volumes;
/// only positive root totals are returned, with the root particle PDG code.
/// Energies remain in Geant4 internal units; this recorder performs no file I/O.

#pragma once

#include "DetectorHit.hh"
#include "record/RecordSchema.hh"
#include "record/TrackAncestry.hh"

#include "G4SystemOfUnits.hh"
#include "globals.hh"

#include <cmath>
#include <stdexcept>
#include <vector>

namespace cupid {

// Energies are unweighted and remain in Geant4 internal units.
struct IncidentEnergyRecord {
  G4int eventId;
  G4int trackId;
  G4int pdg;  // Geant4 PDG encoding of this detector-local incident root.
  G4double edep;
  G4String detectorName;
};

template<> struct RecordTraits<IncidentEnergyRecord> {
  static const RecordDescription<IncidentEnergyRecord>& Get() {
    using R = IncidentEnergyRecord;
    static const auto description = Describe<R>(
        "IncidentEnergy", DetectorKey(&R::detectorName),
        Column("EventID", &R::eventId),
        Column("TrackID", &R::trackId),
        Column("PDG", &R::pdg),
        Column("Edep", &R::edep, keV, "keV"))
        .WithValidation([](const R& record) {
          if (record.eventId < 0 || !std::isfinite(record.edep) || record.edep < 0. || record.trackId <= 0)
            throw std::invalid_argument("IncidentEnergyRecord: invalid record.");
        });
    return description;
  }
};

class IncidentEnergyRecorder {
 public:
  std::vector<IncidentEnergyRecord> Build(G4int eventId, const DetectorHitsCollection& hits,
                                         const TrackAncestry& ancestry) const;
};

}  // namespace cupid
