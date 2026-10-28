/// \file include/DetectorHit.hh
/// \brief Declaration: Store one detector's raw per-track energy deposits for one event.
///
/// LMOSensitiveDetector fills DetectorHit objects in an event-owned hits
/// collection. Recorders read GetTrackDeposits(); energies retain Geant4
/// internal units, including zero-deposit visits needed for ancestry grouping.

#pragma once

#include "G4THitsCollection.hh"
#include "G4VHit.hh"
#include "globals.hh"

#include <map>

namespace cupid {

// One detector's raw per-track deposits in a single event, in Geant4 units.
class DetectorHit final : public G4VHit {
 public:
  explicit DetectorHit(const G4String& detectorName);
  void AddEnergyDeposit(G4int trackId, G4double energyDeposit);
  const G4String& GetDetectorName() const { return fDetectorName; }
  const std::map<G4int, G4double>& GetTrackDeposits() const { return fTrackDeposits; }

 private:
  G4String fDetectorName;
  std::map<G4int, G4double> fTrackDeposits;
};

using DetectorHitsCollection = G4THitsCollection<DetectorHit>;

}  // namespace cupid
