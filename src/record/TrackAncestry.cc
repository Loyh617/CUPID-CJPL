/// \file src/record/TrackAncestry.cc
/// \brief Implementation: Store event-local parent IDs, birth-volume names and PDG codes.
///
/// EventAction owns and clears this table at event start. TrackingAction
/// calls RegisterTrack() when incident recording is enabled, including
/// tracks without deposits. IncidentEnergyRecorder reads it during Build().

#include "record/TrackAncestry.hh"

#include <stdexcept>
#include <string>

namespace cupid {

void TrackAncestry::Clear() { fTracks.clear(); }

void TrackAncestry::RegisterTrack(G4int trackId, G4int parentId, const G4String& birthVolume, G4int pdg) {
  if (trackId <= 0 || parentId < 0 || trackId == parentId || birthVolume.empty())
    throw std::invalid_argument("Invalid track ancestry entry.");
  const auto result = fTracks.emplace(trackId, TrackOrigin{parentId, birthVolume, pdg});
  if (!result.second && (result.first->second.parentId != parentId ||
                         result.first->second.birthVolume != birthVolume || result.first->second.pdg != pdg))
    throw std::runtime_error("Conflicting ancestry for TrackID " + std::to_string(trackId));
}

G4bool TrackAncestry::Contains(G4int trackId) const { return fTracks.count(trackId) != 0; }

const TrackOrigin& TrackAncestry::Get(G4int trackId) const {
  const auto found = fTracks.find(trackId);
  if (found == fTracks.end())
    throw std::runtime_error("Missing ancestry for TrackID " + std::to_string(trackId));
  return found->second;
}

}  // namespace cupid
