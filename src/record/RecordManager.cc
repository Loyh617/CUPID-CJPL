/// \file src/record/RecordManager.cc
/// \brief Bind recorder names to typed builders and aggregate event collections.
///
/// Register each recorder's batch factory, ancestry requirement and Build()
/// adapter here. Selection is validated once; Build() invokes only selected
/// adapters, preserving detector-name uniqueness across all hit collections.

#include "record/RecordManager.hh"
#include "record/EventEnergyRecorder.hh"
#include "record/IncidentEnergyRecorder.hh"

#include "G4HCofThisEvent.hh"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace cupid {
namespace {
template<class Record>
void Append(RecordBatch& batch, const std::vector<Record>& records) {
  auto& destination = *batch.Get<Record>();
  destination.insert(destination.end(), records.begin(), records.end());
}
}

RecordManager::RecordManager(const std::vector<G4String>& names) {
  struct Registration {
    RecordBatch (*create)();
    bool requiresTrackAncestry;
    Builder build;
  };
  static const std::map<G4String, Registration> registry{
      {"EventEnergyRecorder", {&RecordBatch::Create<EventEnergyRecord>, false,
          [](RecordBatch& batch, G4int eventId, const DetectorHitsCollection& hits,
             const TrackAncestry&) {
            Append(batch, EventEnergyRecorder{}.Build(eventId, hits));
          }}},
      {"IncidentEnergyRecorder", {&RecordBatch::Create<IncidentEnergyRecord>, true,
          [](RecordBatch& batch, G4int eventId, const DetectorHitsCollection& hits,
             const TrackAncestry& ancestry) {
            Append(batch, IncidentEnergyRecorder{}.Build(eventId, hits, ancestry));
          }}}};
  std::set<G4String> datasets;
  for (const auto& name : names) {
    const auto found = registry.find(name);
    if (found == registry.end()) throw std::invalid_argument("Unknown recorder: " + name);
    if (IsEnabled(name)) throw std::invalid_argument("Duplicate recorder selection: " + name);
    fNames.push_back(name);
    fBuilders.push_back(found->second.build);
    auto batch = found->second.create();
    if (!datasets.insert(batch.View().Schema().name).second)
      throw std::invalid_argument("RecordManager: selected recorders must use distinct dataset names.");
    fBatches.push_back(std::move(batch));
    fRequiresTrackAncestry = fRequiresTrackAncestry || found->second.requiresTrackAncestry;
  }
}

bool RecordManager::IsEnabled(const G4String& name) const {
  return std::find(fNames.begin(), fNames.end(), name) != fNames.end();
}

void RecordManager::Clear() {
  for (auto& batch : fBatches) batch.Clear();
}

void RecordManager::Build(G4int eventId, G4HCofThisEvent& collections,
                          const TrackAncestry& ancestry) {
  Clear();
  if (eventId < 0) throw std::invalid_argument("RecordManager: negative EventID.");
  if (Empty()) return;
  std::vector<const DetectorHitsCollection*> detectorCollections;
  std::set<G4String> detectorNames;
  for (std::size_t id = 0; id < collections.GetCapacity(); ++id) {
    const auto* hits = dynamic_cast<const DetectorHitsCollection*>(collections.GetHC(id));
    if (!hits) continue;
    for (const auto* hit : *hits->GetVector())
      if (!detectorNames.insert(hit->GetDetectorName()).second)
        throw std::runtime_error("Duplicate detector name across event collections.");
    detectorCollections.push_back(hits);
  }
  for (std::size_t index = 0; index < fBuilders.size(); ++index)
    for (const auto* hits : detectorCollections)
      fBuilders[index](fBatches[index], eventId, *hits, ancestry);
}

}  // namespace cupid
