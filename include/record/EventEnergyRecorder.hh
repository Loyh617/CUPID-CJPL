#pragma once

#include "DetectorHit.hh"

#include "globals.hh"

#include <vector>

namespace cupid {

// Energies are unweighted and remain in Geant4 internal units.
struct EventEnergyRecord {
  G4int eventId;
  G4double edep;
  G4String detectorName;
};

class EventEnergyRecorder {
 public:
  // Emit only detectors with positive total energy; apply no additional energy threshold.
  std::vector<EventEnergyRecord> Build(G4int eventId, const DetectorHitsCollection& hits) const;
};

}  // namespace cupid
