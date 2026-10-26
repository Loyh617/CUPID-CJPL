#pragma once

#include "record/EventEnergyRecorder.hh"
#include "record/IncidentEnergyRecorder.hh"
#include "record/TrackAncestry.hh"

#include "G4UserEventAction.hh"

#include <vector>

namespace cupid {

class EventAction final : public G4UserEventAction {
 public:
  EventAction();
  EventAction(G4bool recordEventEnergy, G4bool recordIncidentEnergy);
  G4bool IsIncidentRecordingEnabled() const { return fRecordIncidentEnergy; }
  void BeginOfEventAction(const G4Event*) override;
  void EndOfEventAction(const G4Event*) override;
  TrackAncestry& GetTrackAncestry() { return fTrackAncestry; }
  const std::vector<EventEnergyRecord>& GetEventRecords() const { return fEventRecords; }
  const std::vector<IncidentEnergyRecord>& GetIncidentRecords() const { return fIncidentRecords; }

 private:
  const G4bool fRecordEventEnergy;
  const G4bool fRecordIncidentEnergy;
  TrackAncestry fTrackAncestry;
  EventEnergyRecorder fEventRecorder;
  IncidentEnergyRecorder fIncidentRecorder;
  std::vector<EventEnergyRecord> fEventRecords;
  std::vector<IncidentEnergyRecord> fIncidentRecords;
};

}  // namespace cupid
