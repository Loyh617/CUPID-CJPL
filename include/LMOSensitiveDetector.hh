#pragma once

#include "G4THitsMap.hh"
#include "G4VSensitiveDetector.hh"

namespace cupid {

// Unweighted energy deposition per crystal for one complete Geant4 event.
class LMOSensitiveDetector final : public G4VSensitiveDetector {
 public:
  LMOSensitiveDetector();
  void Initialize(G4HCofThisEvent* collections) override;

  static constexpr const char* DetectorName = "LMOSD";
  static constexpr const char* CollectionName = "Edep";
  static constexpr const char* CollectionFullName = "LMOSD/Edep";

 protected:
  G4bool ProcessHits(G4Step* step, G4TouchableHistory*) override;

 private:
  G4int fCollectionId = -1;
  // The event owns the collection; a new one is installed for every event.
  G4THitsMap<G4double>* fEnergyDeposits = nullptr;
};

}  // namespace cupid
