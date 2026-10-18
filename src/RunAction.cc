#include "RunAction.hh"
#include "G4Run.hh"
#include "G4ios.hh"

namespace cupid {
void RunAction::EndOfRunAction(const G4Run* run) {
  if (IsMaster()) {
    G4cout << "CUPID_RUN_SUMMARY run=" << run->GetRunID()
           << " events=" << run->GetNumberOfEvent()
           << " aborted=" << (run->GetNumberOfEvent() != run->GetNumberOfEventToBeProcessed())
           << G4endl;
  }
}
}

