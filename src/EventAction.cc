#include "EventAction.hh"
#include "DetectorHit.hh"

#include "G4Event.hh"
#include "G4HCofThisEvent.hh"

#include <stdexcept>
#include <set>

namespace cupid {

// Select the default recorders here, then rebuild the application.
// Arguments: (recordEventEnergy, recordIncidentEnergy).
// (true, false): event totals only; (false, true): incident-particle totals only.
// (true, true): both recorders; (false, false): neither recorder.
// ActionInitialization also uses the incident flag to enable ancestry tracking.
EventAction::EventAction()
    : EventAction(true, false) {}

EventAction::EventAction(G4bool recordEventEnergy, G4bool recordIncidentEnergy)
    : fRecordEventEnergy(recordEventEnergy), fRecordIncidentEnergy(recordIncidentEnergy) {}

void EventAction::BeginOfEventAction(const G4Event*) {
  fTrackAncestry.Clear();
  fEventRecords.clear();
  fIncidentRecords.clear();
}

void EventAction::EndOfEventAction(const G4Event* event) {
  fEventRecords.clear();
  fIncidentRecords.clear();
  if (event->IsAborted() || (!fRecordEventEnergy && !fRecordIncidentEnergy)) return;
  auto* collections = event->GetHCofThisEvent();
  if (!collections) return;

  std::set<G4String> detectorNames;
  for (std::size_t id = 0; id < collections->GetCapacity(); ++id) {
    const auto* hits = dynamic_cast<const DetectorHitsCollection*>(collections->GetHC(id));
    if (!hits) continue;
    for (const auto* hit : *hits->GetVector()) {
      if (!detectorNames.insert(hit->GetDetectorName()).second)
        throw std::runtime_error("Duplicate detector name across event collections.");
    }
    // These selections are independent; an unselected recorder leaves its result vector empty.
    if (fRecordEventEnergy) {
      const auto eventRecords = fEventRecorder.Build(event->GetEventID(), *hits);
      fEventRecords.insert(fEventRecords.end(), eventRecords.begin(), eventRecords.end());
    }
    // Incident attribution requires the ancestry collected by TrackingAction for this event.
    if (fRecordIncidentEnergy) {
      const auto incidentRecords = fIncidentRecorder.Build(event->GetEventID(), *hits, fTrackAncestry);
      fIncidentRecords.insert(fIncidentRecords.end(), incidentRecords.begin(), incidentRecords.end());
    }
  }
}

}  // namespace cupid
