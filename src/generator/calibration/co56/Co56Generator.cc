/// \file src/generator/calibration/co56/Co56Generator.cc
/// \brief Implementation: Reserve a dedicated Co-56 calibration-source generator interface.
///
/// This VGenerator subclass is not connected to GeneratorManager.
/// Generate(event) currently throws: source geometry and emission settings
/// must be implemented before this class can generate calibration events.

#include "generator/calibration/co56/Co56Generator.hh"

#include <stdexcept>

namespace cupid {
void Co56Generator::Generate(G4Event*) {
  throw std::logic_error("Co56Generator: the source model is not implemented.");
}
}
