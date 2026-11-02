/// \file src/EventAction.cc
/// \brief Invoke selected recorders and dispatch typed batches through OutputManager.
///
/// Configure recorder names in DefaultRecorders() and rebuild. Each event
/// clears local state, builds selected records and writes through this worker's
/// RunAction. Output lifetime and file merging remain run-level responsibilities.

#include "EventAction.hh"
#include "RunAction.hh"

#include "G4Event.hh"
#include "G4Threading.hh"
#include "G4ios.hh"

#include <stdexcept>

namespace cupid {

// Configure selected names here and rebuild. ActionInitialization uses this same
// list for the master and workers so that analysis schemas agree before transport.
std::vector<G4String> EventAction::DefaultRecorders() {
  return {"EventEnergyRecorder", "IncidentEnergyRecorder"};
}

EventAction::EventAction(RunAction* runAction)
    : EventAction(runAction ? runAction->GetRecorderNames() : DefaultRecorders(), runAction) {}

EventAction::EventAction(const std::vector<G4String>& recorders, RunAction* runAction)
    : fRunAction(runAction), fRecords(recorders) {
  if (fRunAction && recorders != fRunAction->GetRecorderNames())
    throw std::invalid_argument("EventAction and RunAction recorder selections must match.");
  if (fRunAction && G4Threading::G4GetThreadId() <= 0) {
    G4cout << "CUPID_RECORDERS selected=";
    if (fRecords.Empty()) G4cout << "none";
    for (const auto& name : fRecords.GetNames()) G4cout << name << " ";
    G4cout << G4endl;
  }
}

void EventAction::BeginOfEventAction(const G4Event*) {
  fTrackAncestry.Clear();
  fRecords.Clear();
}

void EventAction::EndOfEventAction(const G4Event* event) {
  fRecords.Clear();
  if (event->IsAborted() || fRecords.Empty()) return;
  auto* collections = event->GetHCofThisEvent();
  if (!collections) return;
  fRecords.Build(event->GetEventID(), *collections, fTrackAncestry);

  if (fRunAction) {
    auto& output = fRunAction->GetOutput();
    const auto runId = fRunAction->GetRunId();
    for (const auto& batch : fRecords.GetBatches()) {
      output.Write(runId, batch.View());
    }
  }
}

}  // namespace cupid
