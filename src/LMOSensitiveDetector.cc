/// \file src/LMOSensitiveDetector.cc
/// \brief Implementation: Collect raw LMO energy deposits without choosing a recording policy.
///
/// DetectorConstruction configures names and attaches this detector to LMO
/// logical volumes in ConstructSDandField(). Geant4 calls Initialize() and
/// ProcessHits(); copy numbers index per-LMO hits consumed by EventAction.

#include "LMOSensitiveDetector.hh"

#include "G4HCofThisEvent.hh"
#include "G4Step.hh"
#include "G4StepPoint.hh"
#include "G4TouchableHandle.hh"
#include "G4Track.hh"

#include <stdexcept>
#include <set>

namespace cupid {

LMOSensitiveDetector::LMOSensitiveDetector() : G4VSensitiveDetector(DetectorName) {
  collectionName.insert(CollectionName);
}

void LMOSensitiveDetector::SetDetectorNames(const std::vector<G4String>& names) {
  std::set<G4String> unique;
  for (const auto& name : names) {
    if (name.empty() || !unique.insert(name).second)
      throw std::invalid_argument("LMO detector names must be nonempty and unique.");
  }
  fDetectorNames = names;
}

void LMOSensitiveDetector::Initialize(G4HCofThisEvent* collections) {
  if (fDetectorNames.empty()) throw std::runtime_error("LMO detector names were not configured.");
  fHits = new DetectorHitsCollection(SensitiveDetectorName, collectionName[0]);
  if (fCollectionId < 0) fCollectionId = GetCollectionID(0);
  collections->AddHitsCollection(fCollectionId, fHits);
  // Keep stable per-detector hit slots, including unhit detectors. Recorders decide
  // which aggregates to output; current energy recorders omit zero totals.
  for (const auto& name : fDetectorNames) fHits->insert(new DetectorHit(name));
}

G4bool LMOSensitiveDetector::ProcessHits(G4Step* step, G4TouchableHistory*) {
  // Attribute this step to its pre-step volume, including a step ending at a boundary.
  const auto lmoId = step->GetPreStepPoint()->GetTouchableHandle()->GetCopyNumber();
  if (lmoId < 0 || static_cast<std::size_t>(lmoId) >= fHits->entries())
    throw std::runtime_error("LMO copy number is outside the configured detector list.");
  (*fHits)[lmoId]->AddEnergyDeposit(step->GetTrack()->GetTrackID(), step->GetTotalEnergyDeposit());
  return true;
}

}  // namespace cupid
