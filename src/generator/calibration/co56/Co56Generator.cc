#include "generator/calibration/co56/Co56Generator.hh"

namespace cupid {
Co56Generator::Co56Generator(const GeneratorConfig& config, const GeometryManager& geometry)
    : fVertices(config, geometry) {}
void Co56Generator::Generate(G4Event* event) {
  fEmitter.Emit(event, 27, 56, fVertices.Sample());
}
}
