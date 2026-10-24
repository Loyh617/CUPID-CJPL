#include "generator/calibration/th232/Th232Generator.hh"

#include <stdexcept>

namespace cupid {
void Th232Generator::Generate(G4Event*) {
  throw std::logic_error("Th232Generator: the source model is not implemented.");
}
}
