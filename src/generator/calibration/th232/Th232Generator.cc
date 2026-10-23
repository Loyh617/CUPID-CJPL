#include "generator/calibration/th232/Th232Generator.hh"

namespace cupid {
Th232Generator::Th232Generator(const GeneratorConfig& config, const GeometryManager& geometry)
    : fVertices(config, geometry) {}
void Th232Generator::Generate(G4Event* event) {
  fEmitter.Emit(event, 90, 232, fVertices.Sample());
}
}
