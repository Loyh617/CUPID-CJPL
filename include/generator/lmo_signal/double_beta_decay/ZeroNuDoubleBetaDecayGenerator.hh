/// \file include/generator/lmo_signal/double_beta_decay/ZeroNuDoubleBetaDecayGenerator.hh
/// \brief Declaration: Reserve the neutrinoless double-beta primary-generation interface.
///
/// Implement Generate(event) with an explicitly chosen signal model and
/// source-position sampler before connecting it to GeneratorManager.
/// The current placeholder throws and supplies no decay spectrum.

#pragma once
#include "generator/VGenerator.hh"

namespace cupid {
class ZeroNuDoubleBetaDecayGenerator final : public VGenerator {
 public:
  void Generate(G4Event*) override;
};
}
