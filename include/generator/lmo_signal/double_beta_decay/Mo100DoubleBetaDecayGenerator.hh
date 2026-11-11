/// Generate one ground-state Mo-100 double-beta decay in the enabled LMO array.
/// Use EventEnergyRecorder for the per-event energy deposited in each LMO by both electrons and their secondaries.
#pragma once

#include "generator/VGenerator.hh"
#include "generator/toolkit/sampler/DirectVolumeSampler.hh"

#include <cstddef>
#include <optional>
#include <vector>

namespace cupid {
class GeometryManager;

class Mo100DoubleBetaDecayGenerator final : public VGenerator {
 public:
  enum class Mode { TwoNu, ZeroNu };
  Mo100DoubleBetaDecayGenerator(const GeometryManager& geometry, Mode mode);
  void Generate(G4Event* event) override;

 private:
  G4ThreeVector SamplePosition();
  struct Source {
    DirectVolumeSampler sampler;
    G4ThreeVector halfLengths;
  };
  const GeometryManager& fGeometry;
  const Mode fMode;
  std::optional<std::size_t> fGeneration;
  std::vector<Source> fSources;
};
}  // namespace cupid
