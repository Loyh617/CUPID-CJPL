/// \file include/generator/calibration/co56/Co56Generator.hh
/// \brief Declaration: Reserve a dedicated Co-56 calibration-source generator interface.
///
/// This VGenerator subclass is not connected to GeneratorManager.
/// Generate(event) currently throws: source geometry and emission settings
/// must be implemented before this class can generate calibration events.

#pragma once

#include "generator/VGenerator.hh"

namespace cupid {
class Co56Generator final : public VGenerator {
 public:
  void Generate(G4Event*) override;
};
}
