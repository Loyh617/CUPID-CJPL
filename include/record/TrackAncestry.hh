/// \file include/record/TrackAncestry.hh
/// \brief Declaration: Store event-local parent IDs, birth-volume names and PDG codes.
///
/// EventAction owns and clears this table at event start. TrackingAction
/// calls RegisterTrack() when incident recording is enabled, including
/// tracks without deposits. IncidentEnergyRecorder reads it during Build().

#pragma once

#include "globals.hh"

#include <unordered_map>

namespace cupid {

struct TrackOrigin {
  G4int parentId;
  G4String birthVolume;
  G4int pdg;
};

// Event-local bookkeeping, including tracks with no energy deposition.
class TrackAncestry {
 public:
  void Clear();
  void RegisterTrack(G4int trackId, G4int parentId, const G4String& birthVolume, G4int pdg);
  G4bool Contains(G4int trackId) const;
  const TrackOrigin& Get(G4int trackId) const;

 private:
  std::unordered_map<G4int, TrackOrigin> fTracks;
};

}  // namespace cupid
