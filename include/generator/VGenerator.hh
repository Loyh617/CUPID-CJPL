/// \file include/generator/VGenerator.hh
/// \brief Declaration: Declare the minimal interface for dedicated primary generators.
///
/// Implement Generate(G4Event*) to add primary vertices to the supplied event.
/// Concrete generators choose their own sampling tools. Dedicated generators
/// are currently placeholders and are not dispatched by GeneratorManager.

#pragma once
class G4Event;
namespace cupid {
class VGenerator {
 public:
  virtual ~VGenerator() = default;
  virtual void Generate(G4Event*) = 0;
};
}
