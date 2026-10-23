#pragma once
#include <memory>
class G4Event;
class G4ParticleGun;
namespace cupid {
struct GeneratorConfig;
class GeometryManager;
class VGenerator;
class GeneratorManager {
 public:
  GeneratorManager(const GeneratorConfig&, const GeometryManager&);
  ~GeneratorManager();
  void Generate(G4Event*);
  static void Check(const GeneratorConfig&, const GeometryManager&);
  static void List(const GeneratorConfig&);

 private:
  const GeometryManager& fGeometry;
  const GeneratorConfig& fConfig;
  std::unique_ptr<G4ParticleGun> fGun;
  std::unique_ptr<VGenerator> fGenerator;
};
}
