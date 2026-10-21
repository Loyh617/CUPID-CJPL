#include "RunAction.hh"
#include "G4Run.hh"
#include "G4ios.hh"

namespace cupid {
// Retired 2026-10-21: RunAction now exposes empty lifecycle hooks.
// Restore only if a custom run summary is requested again.
// void RunAction::EndOfRunAction(const G4Run* run) {
//   if (IsMaster()) {
//     G4cout << "CUPID_RUN_SUMMARY run=" << run->GetRunID()
//            << " events=" << run->GetNumberOfEvent()
//            << " aborted=" << (run->GetNumberOfEvent() != run->GetNumberOfEventToBeProcessed())
//            << G4endl;
//   }
// }

void RunAction::BeginOfRunAction(const G4Run*) {
  // Reserved for future implementation.
}

void RunAction::EndOfRunAction(const G4Run*) {
  // Reserved for future implementation.
}
}

