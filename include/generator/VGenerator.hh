#pragma once
class G4Event;
namespace cupid {
class VGenerator {
 public:
  virtual ~VGenerator() = default;
  virtual void Generate(G4Event*) = 0;
};
}
