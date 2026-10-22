#include "generator/GeneratorManager.hh"
#include "generator/GeneratorConfig.hh"
#include "generator/VGenerator.hh"
#include "generator/common/VertexSampler.hh"
#include "generator/calibration/th232/Th232Generator.hh"
#include "generator/calibration/co56/Co56Generator.hh"
#include "G4Event.hh"
#include "G4Gamma.hh"
#include "G4ParticleDefinition.hh"
#include "G4ParticleGun.hh"
#include "G4PrimaryParticle.hh"
#include "G4PrimaryVertex.hh"
#include "G4ios.hh"
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace cupid {
namespace {
void CheckSelection(const GeneratorConfig& config) {
  if (config.selection != "gun" && config.selection != "calibration/th232" &&
      config.selection != "calibration/co56")
    throw std::runtime_error("Unsupported generator: " + config.selection);
}
}
GeneratorManager::GeneratorManager(const GeneratorConfig& config)
    : fConfig(config), fGun(std::make_unique<G4ParticleGun>(1)) {
  fGun->SetParticleDefinition(G4Gamma::GammaDefinition());
  fGun->SetParticleEnergy(1.*MeV);
  fGun->SetParticlePosition({0., -350.*mm, -100.*mm});
  fGun->SetParticleMomentumDirection({0., 0., 1.});
}
GeneratorManager::~GeneratorManager() = default;
void GeneratorManager::Check(const GeneratorConfig& config) {
  CheckSelection(config);
  if (config.selection != "gun") {
    VertexSampler sampler(config);
  }
}
void GeneratorManager::List(const GeneratorConfig& config) {
  G4cout << "CUPID_GENERATOR selected=" << config.selection;
  if (config.selection == "gun") {
    G4cout << " configuration=/gun/" << G4endl;
    return;
  }
  G4cout << " position=" << config.positionMode;
  if (config.positionMode == "point") G4cout << " point_mm=" << config.point/mm;
  else G4cout << " volume=" << config.volume;
  G4cout << " decay=fullChain primary=neutral_ground_state_ion_at_rest"
            " normalization=per_initial_parent history_t0=ion_creation" << G4endl;
}
void GeneratorManager::Generate(G4Event* event) {
  if (fConfig.selection == "gun") {
    fGun->GeneratePrimaryVertex(event);
  } else {
    if (!fGenerator) {
      CheckSelection(fConfig);
      if (fConfig.selection == "calibration/th232")
        fGenerator = std::make_unique<Th232Generator>(fConfig);
      else fGenerator = std::make_unique<Co56Generator>(fConfig);
    }
    fGenerator->Generate(event);
  }
  if (fConfig.verbosity > 0) {
    auto* vertex = event->GetPrimaryVertex();
    auto* particle = vertex->GetPrimary();
    std::ostringstream line;
    line << std::setprecision(12) << "CUPID_PRIMARY event=" << event->GetEventID()
         << " source=" << fConfig.selection << " particle=" << particle->GetG4code()->GetParticleName()
         << " x_mm=" << vertex->GetX0()/mm << " y_mm=" << vertex->GetY0()/mm
         << " z_mm=" << vertex->GetZ0()/mm << " kinetic_MeV=" << particle->GetKineticEnergy()/MeV;
    G4cout << line.str() << G4endl;
  }
}
}
