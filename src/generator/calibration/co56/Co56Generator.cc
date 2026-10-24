#include "generator/calibration/co56/Co56Generator.hh"

#include <stdexcept>

namespace cupid {
void Co56Generator::Generate(G4Event*) {
  throw std::logic_error("Co56Generator: the source model is not implemented.");
}
}
