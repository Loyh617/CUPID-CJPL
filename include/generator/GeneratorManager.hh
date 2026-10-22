#pragma once
#include <memory>
class G4Event;
class G4ParticleGun;
namespace cupid {
struct GeneratorConfig;
class VGenerator;
class GeneratorManager {
 public:
  explicit GeneratorManager(const GeneratorConfig&);
  ~GeneratorManager();
  void Generate(G4Event*);
  static void Check(const GeneratorConfig&);
  static void List(const GeneratorConfig&);

 private:
  const GeneratorConfig& fConfig;
  std::unique_ptr<G4ParticleGun> fGun;
  std::unique_ptr<VGenerator> fGenerator;
};
}
