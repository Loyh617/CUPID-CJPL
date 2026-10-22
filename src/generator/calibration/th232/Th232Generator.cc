#include "generator/calibration/th232/Th232Generator.hh"

namespace cupid {
Th232Generator::Th232Generator(const GeneratorConfig& config) : fVertices(config) {}
void Th232Generator::Generate(G4Event* event) {
  fEmitter.Emit(event, 90, 232, fVertices.Sample());
}
}
