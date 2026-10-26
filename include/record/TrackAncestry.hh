#pragma once

#include "globals.hh"

#include <unordered_map>

namespace cupid {

struct TrackOrigin {
  G4int parentId;
  G4String birthVolume;
};

// Event-local bookkeeping, including tracks with no energy deposition.
class TrackAncestry {
 public:
  void Clear();
  void RegisterTrack(G4int trackId, G4int parentId, const G4String& birthVolume);
  G4bool Contains(G4int trackId) const;
  const TrackOrigin& Get(G4int trackId) const;

 private:
  std::unordered_map<G4int, TrackOrigin> fTracks;
};

}  // namespace cupid
