/// \file src/DetectorHit.cc
/// \brief Implementation: Store one detector's raw per-track energy deposits for one event.
///
/// LMOSensitiveDetector fills DetectorHit objects in an event-owned hits
/// collection. Recorders read GetTrackDeposits(); energies retain Geant4
/// internal units, including zero-deposit visits needed for ancestry grouping.

#include "DetectorHit.hh"

#include <cmath>
#include <stdexcept>

namespace cupid {

DetectorHit::DetectorHit(const G4String& detectorName) : fDetectorName(detectorName) {
  if (detectorName.empty()) throw std::invalid_argument("A detector hit needs a volume name.");
}

void DetectorHit::AddEnergyDeposit(G4int trackId, G4double energyDeposit) {
  if (trackId <= 0 || !std::isfinite(energyDeposit) || energyDeposit < 0.)
    throw std::invalid_argument("Invalid track ID or energy deposit.");
  // Zero-deposit visits retain the identity of an incident particle or intermediate ancestor.
  fTrackDeposits[trackId] += energyDeposit;
}

}  // namespace cupid
