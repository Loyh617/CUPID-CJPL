#include "LMOSensitiveDetector.hh"

#include "G4HCofThisEvent.hh"
#include "G4Step.hh"
#include "G4StepPoint.hh"
#include "G4TouchableHandle.hh"

namespace cupid {

LMOSensitiveDetector::LMOSensitiveDetector()
    : G4VSensitiveDetector(DetectorName) {
  collectionName.insert(CollectionName);
}

void LMOSensitiveDetector::Initialize(G4HCofThisEvent* collections) {
  fEnergyDeposits = new G4THitsMap<G4double>(SensitiveDetectorName, collectionName[0]);
  if (fCollectionId < 0) fCollectionId = GetCollectionID(0);
  collections->AddHitsCollection(fCollectionId, fEnergyDeposits);
}

G4bool LMOSensitiveDetector::ProcessHits(G4Step* step, G4TouchableHistory*) {
  auto energyDeposit = step->GetTotalEnergyDeposit();
  if (energyDeposit == 0.) return false;

  // Depth zero is the crystal placement, including for steps leaving it.
  const auto lmoId = step->GetPreStepPoint()->GetTouchableHandle()->GetCopyNumber();
  fEnergyDeposits->add(lmoId, energyDeposit);
  return true;
}

}  // namespace cupid
