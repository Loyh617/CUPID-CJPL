/// \file include/generator/lmo_signal/double_beta_decay/TwoNuDoubleBetaDecayGenerator.hh
/// \brief Declaration: Reserve the two-neutrino double-beta primary-generation interface.
///
/// Implement Generate(event) with a specified decay-kinematics model and
/// source-position sampler before connecting it to GeneratorManager.
/// The current placeholder throws and supplies no decay spectrum.

#pragma once
#include "generator/VGenerator.hh"

namespace cupid {
class TwoNuDoubleBetaDecayGenerator final : public VGenerator {
 public:
  void Generate(G4Event*) override;
};
}
