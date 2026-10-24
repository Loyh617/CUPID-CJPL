#pragma once

#include "generator/VGenerator.hh"

namespace cupid {
class Th232Generator final : public VGenerator {
 public:
  void Generate(G4Event*) override;
};
}
