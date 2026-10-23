#pragma once
#include "generator/VGenerator.hh"
#include "generator/common/VertexSampler.hh"
#include "generator/common/IonEmitter.hh"

namespace cupid {
class Co56Generator final : public VGenerator {
 public:
  Co56Generator(const GeneratorConfig&, const GeometryManager&);
  void Generate(G4Event*) override;

 private:
  VertexSampler fVertices;
  IonEmitter fEmitter;
};
}
