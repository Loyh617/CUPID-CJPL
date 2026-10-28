/// \file include/EventAction.hh
/// \brief Coordinate named recorders and pass their typed results to output.
///
/// ActionInitialization supplies the same thread's RunAction. Geant4 invokes
/// BeginOfEventAction() to reset state and EndOfEventAction() to build records.
/// Constructor names select recorders; omitting RunAction permits in-memory use.

#pragma once

#include "record/RecordManager.hh"
#include "record/TrackAncestry.hh"

#include "G4UserEventAction.hh"

#include <vector>

namespace cupid {

class RunAction;

class EventAction final : public G4UserEventAction {
 public:
  static std::vector<G4String> DefaultRecorders();
  explicit EventAction(RunAction* runAction = nullptr);
  EventAction(const std::vector<G4String>& recorders, RunAction* runAction = nullptr);
  G4bool RequiresTrackAncestry() const { return fRecords.RequiresTrackAncestry(); }
  void BeginOfEventAction(const G4Event*) override;
  void EndOfEventAction(const G4Event*) override;
  TrackAncestry& GetTrackAncestry() { return fTrackAncestry; }
  const std::vector<EventEnergyRecord>& GetEventRecords() const {
    return fRecords.GetRecords<EventEnergyRecord>();
  }
  const std::vector<IncidentEnergyRecord>& GetIncidentRecords() const {
    return fRecords.GetRecords<IncidentEnergyRecord>();
  }

 private:
  RunAction* fRunAction = nullptr;  // Non-owning; managed by Geant4.
  RecordManager fRecords;
  TrackAncestry fTrackAncestry;
};

}  // namespace cupid
