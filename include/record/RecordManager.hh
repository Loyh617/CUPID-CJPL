/// \file include/record/RecordManager.hh
/// \brief Select named recorders and retain their typed results for one event.
///
/// EventAction owns this manager and calls Clear() and Build(). GetBatches()
/// exposes original record vectors in a type-safe variant for Action dispatch.
/// The manager has no dependency on output modules and performs no file I/O.

#pragma once

#include "record/EventEnergyRecorder.hh"
#include "record/IncidentEnergyRecorder.hh"
#include "record/TrackAncestry.hh"

#include <variant>
#include <vector>

class G4HCofThisEvent;

namespace cupid {

using RecordBatch = std::variant<std::vector<EventEnergyRecord>,
                                 std::vector<IncidentEnergyRecord>>;

class RecordManager final {
 public:
  explicit RecordManager(const std::vector<G4String>& names);
  bool IsEnabled(const G4String& name) const;
  bool Empty() const { return fNames.empty(); }
  bool RequiresTrackAncestry() const { return fRequiresTrackAncestry; }
  const std::vector<G4String>& GetNames() const { return fNames; }
  void Clear();
  void Build(G4int eventId, G4HCofThisEvent& collections,
             const TrackAncestry& ancestry);
  const std::vector<RecordBatch>& GetBatches() const { return fBatches; }

  template<class Record>
  const std::vector<Record>& GetRecords() const {
    for (const auto& batch : fBatches)
      if (const auto* rows = std::get_if<std::vector<Record>>(&batch)) return *rows;
    static const std::vector<Record> empty;
    return empty;
  }

 private:
  using Builder = void (*)(RecordBatch&, G4int, const DetectorHitsCollection&,
                           const TrackAncestry&);
  std::vector<G4String> fNames;
  std::vector<Builder> fBuilders;
  std::vector<RecordBatch> fBatches;
  bool fRequiresTrackAncestry = false;
};

}  // namespace cupid
