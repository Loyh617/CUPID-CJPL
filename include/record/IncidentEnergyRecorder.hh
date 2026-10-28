/// \file include/record/IncidentEnergyRecorder.hh
/// \brief Declaration: Group detector energy deposits by their incident ancestor track.
///
/// EventAction calls Build(eventId, hits, ancestry) after tracking completes.
/// Grouping is detector-local and follows parent IDs and birth volumes;
/// zero-deposit roots are retained. Returned IncidentEnergyRecord energies
/// remain in Geant4 internal units; this recorder performs no file I/O.

#pragma once

#include "DetectorHit.hh"
#include "record/TrackAncestry.hh"

#include "globals.hh"

#include <vector>

namespace cupid {

// Energies are unweighted and remain in Geant4 internal units.
struct IncidentEnergyRecord {
  G4int eventId;
  G4int trackId;
  G4double edep;
  G4String detectorName;
};

class IncidentEnergyRecorder {
 public:
  std::vector<IncidentEnergyRecord> Build(G4int eventId, const DetectorHitsCollection& hits,
                                         const TrackAncestry& ancestry) const;
};

}  // namespace cupid
