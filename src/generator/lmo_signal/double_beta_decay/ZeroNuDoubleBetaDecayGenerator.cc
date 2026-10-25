#include "generator/lmo_signal/double_beta_decay/ZeroNuDoubleBetaDecayGenerator.hh"
#include <stdexcept>

namespace cupid {
void ZeroNuDoubleBetaDecayGenerator::Generate(G4Event*) {
  throw std::logic_error("ZeroNuDoubleBetaDecayGenerator: the decay model is not implemented.");
}
}
