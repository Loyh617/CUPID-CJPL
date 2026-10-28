/// \file include/generator/calibration/th232/Th232Generator.hh
/// \brief Declaration: Reserve a dedicated Th-232 calibration-source generator interface.
///
/// This VGenerator subclass is not connected to GeneratorManager.
/// Generate(event) currently throws. Existing Th-232 test macros instead
/// configure the gun; they do not use this placeholder source model.

#pragma once

#include "generator/VGenerator.hh"

namespace cupid {
class Th232Generator final : public VGenerator {
 public:
  void Generate(G4Event*) override;
};
}
