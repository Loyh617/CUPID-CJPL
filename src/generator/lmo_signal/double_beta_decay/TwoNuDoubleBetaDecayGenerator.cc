#include "generator/lmo_signal/double_beta_decay/TwoNuDoubleBetaDecayGenerator.hh"
#include <stdexcept>

namespace cupid {
void TwoNuDoubleBetaDecayGenerator::Generate(G4Event*) {
  throw std::logic_error("TwoNuDoubleBetaDecayGenerator: the decay model is not implemented.");
}
}
