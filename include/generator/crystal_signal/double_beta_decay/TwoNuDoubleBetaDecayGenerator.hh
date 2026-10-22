#pragma once
#include "generator/VGenerator.hh"

namespace cupid {
class TwoNuDoubleBetaDecayGenerator final : public VGenerator {
 public:
  void Generate(G4Event*) override;
};
}
