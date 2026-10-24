#pragma once

#include "generator/VGenerator.hh"

namespace cupid {
class Co56Generator final : public VGenerator {
 public:
  void Generate(G4Event*) override;
};
}
