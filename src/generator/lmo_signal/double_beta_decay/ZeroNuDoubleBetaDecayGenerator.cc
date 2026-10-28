/// \file src/generator/lmo_signal/double_beta_decay/ZeroNuDoubleBetaDecayGenerator.cc
/// \brief Implementation: Reserve the neutrinoless double-beta primary-generation interface.
///
/// Implement Generate(event) with an explicitly chosen signal model and
/// source-position sampler before connecting it to GeneratorManager.
/// The current placeholder throws and supplies no decay spectrum.

#include "generator/lmo_signal/double_beta_decay/ZeroNuDoubleBetaDecayGenerator.hh"
#include <stdexcept>

namespace cupid {
void ZeroNuDoubleBetaDecayGenerator::Generate(G4Event*) {
  throw std::logic_error("ZeroNuDoubleBetaDecayGenerator: the decay model is not implemented.");
}
}
