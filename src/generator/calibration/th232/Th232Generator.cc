/// \file src/generator/calibration/th232/Th232Generator.cc
/// \brief Implementation: Reserve a dedicated Th-232 calibration-source generator interface.
///
/// This VGenerator subclass is not connected to GeneratorManager.
/// Generate(event) currently throws. Existing Th-232 test macros instead
/// configure the gun; they do not use this placeholder source model.

#include "generator/calibration/th232/Th232Generator.hh"

#include <stdexcept>

namespace cupid {
void Th232Generator::Generate(G4Event*) {
  throw std::logic_error("Th232Generator: the source model is not implemented.");
}
}
