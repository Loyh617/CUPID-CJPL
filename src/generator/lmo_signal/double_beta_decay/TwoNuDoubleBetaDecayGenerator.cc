/// \file src/generator/lmo_signal/double_beta_decay/TwoNuDoubleBetaDecayGenerator.cc
/// \brief Implementation: Reserve the two-neutrino double-beta primary-generation interface.
///
/// Implement Generate(event) with a specified decay-kinematics model and
/// source-position sampler before connecting it to GeneratorManager.
/// The current placeholder throws and supplies no decay spectrum.

#include "generator/lmo_signal/double_beta_decay/TwoNuDoubleBetaDecayGenerator.hh"
#include <stdexcept>

namespace cupid {
void TwoNuDoubleBetaDecayGenerator::Generate(G4Event*) {
  throw std::logic_error("TwoNuDoubleBetaDecayGenerator: the decay model is not implemented.");
}
}
