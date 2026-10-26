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
