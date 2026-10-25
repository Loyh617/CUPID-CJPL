#pragma once
#include "generator/VGenerator.hh"

namespace cupid {
class ZeroNuDoubleBetaDecayGenerator final : public VGenerator {
 public:
  void Generate(G4Event*) override;
};
}
