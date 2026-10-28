/// \file src/EventAction.cc
/// \brief Invoke selected recorders and dispatch typed batches through OutputManager.
///
/// Configure recorder names in the default constructor and rebuild. Each event
/// clears local state, builds selected records and writes through this worker's
/// RunAction. Output lifetime and file merging remain run-level responsibilities.

#include "EventAction.hh"
#include "RunAction.hh"

#include "G4Event.hh"
#include "G4Threading.hh"
#include "G4ios.hh"

#include <variant>

namespace cupid {

// Select recorders by their registered names, then rebuild the application.
// Keep either name for one mode, both for both modes, or an empty vector for neither.
// ActionInitialization enables ancestry tracking when any selection requires it.
EventAction::EventAction(RunAction* runAction)
    : EventAction({"EventEnergyRecorder", "IncidentEnergyRecorder"}, runAction) {}

EventAction::EventAction(const std::vector<G4String>& recorders, RunAction* runAction)
    : fRunAction(runAction), fRecords(recorders) {
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
      // The visitor passes the original vector by const reference to its Write overload.
      std::visit([&](const auto& records) { output.Write(runId, records); }, batch);
    }
  }
}

}  // namespace cupid
