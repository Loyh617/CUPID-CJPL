/// \file include/record/RecordManager.hh
/// \brief Select named recorders and own their result vectors for one event.
///
/// EventAction calls Clear() and Build(), then reads GetEventRecords() or
/// GetIncidentRecords(). Add a new recorder's selection, storage and Build call
/// explicitly here and in the implementation. The manager performs no file I/O.

#pragma once

#include "record/EventEnergyRecorder.hh"
#include "record/IncidentEnergyRecorder.hh"
#include "record/TrackAncestry.hh"

#include <vector>

class G4HCofThisEvent;

namespace cupid {
class RecordManager final {
 public:
  explicit RecordManager(const std::vector<G4String>& names);
  bool IsEnabled(const G4String& name) const;
  bool Empty() const { return fNames.empty(); }
  bool RequiresTrackAncestry() const { return fIncidents; }
  const std::vector<G4String>& GetNames() const { return fNames; }
  void Clear();
  void Build(G4int eventId, G4HCofThisEvent& collections, const TrackAncestry& ancestry);
  const std::vector<EventEnergyRecord>& GetEventRecords() const { return fEventRecords; }
  const std::vector<IncidentEnergyRecord>& GetIncidentRecords() const { return fIncidentRecords; }

 private:
  std::vector<G4String> fNames;
  bool fEvents = false;
  bool fIncidents = false;
  std::vector<EventEnergyRecord> fEventRecords;
  std::vector<IncidentEnergyRecord> fIncidentRecords;
};
}  // namespace cupid
