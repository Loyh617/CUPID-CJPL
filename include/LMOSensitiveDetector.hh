/// \file include/LMOSensitiveDetector.hh
/// \brief Declaration: Collect raw LMO energy deposits without choosing a recording policy.
///
/// DetectorConstruction configures names and attaches this detector to LMO
/// logical volumes in ConstructSDandField(). Geant4 calls Initialize() and
/// ProcessHits(); copy numbers index per-LMO hits consumed by EventAction.

#pragma once

#include "DetectorHit.hh"

#include "G4VSensitiveDetector.hh"

#include <vector>

namespace cupid {

// Collect raw per-track deposits; recording policies belong to record classes.
class LMOSensitiveDetector final : public G4VSensitiveDetector {
 public:
  LMOSensitiveDetector();
  void SetDetectorNames(const std::vector<G4String>& names);
  void Initialize(G4HCofThisEvent* collections) override;

  static constexpr const char* DetectorName = "LMOSD";
  static constexpr const char* CollectionName = "Edep";
  static constexpr const char* CollectionFullName = "LMOSD/Edep";

 protected:
  G4bool ProcessHits(G4Step* step, G4TouchableHistory*) override;

 private:
  G4int fCollectionId = -1;
  std::vector<G4String> fDetectorNames;
  // Owned by the current event.
  DetectorHitsCollection* fHits = nullptr;
};

}  // namespace cupid
