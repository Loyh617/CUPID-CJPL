/// \file src/PhysicsList.cc
/// \brief Implementation: Assemble the modular decay, electromagnetic and nuclear transport physics.
///
/// Register PhysicsList with the run manager in main() before initialization.
/// Its constructor registers individual physics constructors and configures
/// atomic relaxation, radioactive-decay time limits and production cuts.
/// Double-beta primary kinematics must be supplied by a generator.

#include "PhysicsList.hh"

// Electromagnetic interactions.
#include "G4EmLivermorePhysics.hh"

// Photon-, electron-, positron- and muon-induced nuclear interactions.
#include "G4EmExtraPhysics.hh"

// Particle decays and radioactive nuclear decays.
#include "G4DecayPhysics.hh"
#include "G4RadioactiveDecayPhysics.hh"

// Hadron elastic scattering, inelastic reactions and neutron processes.
#include "G4HadronElasticPhysicsHP.hh"
#include "G4HadronPhysicsShielding.hh"

// Nuclear capture of stopped particles, including negative muons.
#include "G4StoppingPhysics.hh"

// Ion elastic and inelastic nuclear interactions.
#include "G4IonElasticPhysics.hh"
#include "G4IonQMDPhysics.hh"

// Configuration parameters and units.
#include "G4EmParameters.hh"
#include "G4HadronicParameters.hh"
#include "G4ParticleHPManager.hh"
#include "G4SystemOfUnits.hh"

namespace cupid {

PhysicsList::PhysicsList(G4int verbosity)
    : G4VModularPhysicsList() {
  SetVerboseLevel(verbosity);

  // 1. Electromagnetic transport, with low-energy Livermore models.
  RegisterPhysics(new G4EmLivermorePhysics(verbosity));

  // 2. Gamma-, electro- and muon-nuclear processes are enabled by default
  // in Geant4 11.4.1. No additional switches are needed here.
  RegisterPhysics(new G4EmExtraPhysics(verbosity));

  // 3. Decays of unstable particles such as muons.
  RegisterPhysics(new G4DecayPhysics(verbosity));

  // 4. Radioactive decays. Register this after electromagnetic physics.
  RegisterPhysics(new G4RadioactiveDecayPhysics(verbosity));

  // 5. Hadron elastic scattering, including low-energy neutron HP models.
  RegisterPhysics(new G4HadronElasticPhysicsHP(verbosity));

  // 6. Hadron inelastic reactions, neutron capture and neutron fission.
  // This is one physics constructor, not the complete Shielding list.
  RegisterPhysics(new G4HadronPhysicsShielding(verbosity));
  G4ParticleHPManager::GetInstance()->SetProduceFissionFragments(true);

  // 7. At-rest nuclear capture and annihilation processes.
  RegisterPhysics(new G4StoppingPhysics(verbosity));

  // 8. Ion elastic nuclear interactions.
  RegisterPhysics(new G4IonElasticPhysics(verbosity));

  // 9. Ion inelastic nuclear interactions.
  RegisterPhysics(new G4IonQMDPhysics(verbosity));

  // Configure atomic relaxation after creating the EM constructor,
  // which resets EM parameters to its own defaults.
  auto* emParameters = G4EmParameters::Instance();
  emParameters->SetFluo(true);
  emParameters->SetAuger(true);
  emParameters->SetDeexcitationIgnoreCut(true);

  // Permit long-lived radioactive chains such as Th-232.
  // This is a simulation time cutoff, not a change to nuclear lifetimes
  // or a detector integration window.
  G4HadronicParameters::Instance()
      ->SetTimeThresholdForRadioactiveDecay(1.0e60 * CLHEP::year);

  // Provisional production range cuts; validate convergence for each study.
  // These are neither tracking cuts nor maximum step lengths.
  SetDefaultCutValue(0.1 * mm);
  SetCutValue(0.0 * mm, "proton");
}

}  // namespace cupid
