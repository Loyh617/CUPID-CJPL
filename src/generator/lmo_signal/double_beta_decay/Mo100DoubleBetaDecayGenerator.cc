/// Ground-state allowed 2nu phase space and light-mass 0nu kinematics.
/// Spectrum conventions: Kotila & Iachello, Phys. Rev. C 85, 034316;
/// baseline energy/angle factors as in DECAY0 modes 4 and 1, with G4 Coulomb weights.
/// Energies in the sampler are kinetic energies in Geant4 units.
#include "generator/lmo_signal/double_beta_decay/Mo100DoubleBetaDecayGenerator.hh"
#include "geometry/GeometryManager.hh"

#include "G4BetaDecayCorrections.hh"
#include "G4Electron.hh"
#include "G4Event.hh"
#include "G4PhysicalConstants.hh"
#include "G4PrimaryParticle.hh"
#include "G4PrimaryVertex.hh"
#include "G4RandomDirection.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>

namespace cupid {
namespace {
constexpr double qValue = 3034.40 * keV;  // Mo-100 to Ru-100(gs), neglecting recoil.
using Energies = std::pair<double, double>;

double Momentum(double kinetic) {
  return std::sqrt(kinetic * (kinetic + 2 * electron_mass_c2));
}

// Linearly interpolate p*E*F on a uniform grid (~0.74 keV spacing).
// Normalize to the largest grid value, a bound on every interpolated weight.
class ElectronSpectrum {
 public:
  ElectronSpectrum() {
    G4BetaDecayCorrections correction(44, 100);  // Daughter charge and mass number.
    for (std::size_t i = 0; i <= intervals; ++i) {
      // Evaluate the finite p*F limit just above zero to avoid F(W=1).
      const double t = std::max(qValue * i / intervals, 1.e-6 * keV);
      const double e = electron_mass_c2 + t;
      fWeight[i] = Momentum(t) * e * correction.FermiFunction(e / electron_mass_c2);
      if (!std::isfinite(fWeight[i]) || fWeight[i] <= 0.)
        throw std::runtime_error("Invalid Mo-100 Coulomb weight.");
    }
    const double maximum = *std::max_element(fWeight.begin(), fWeight.end());
    for (auto& value : fWeight) value /= maximum;
  }

  double Weight(double t) const {
    const double x = t * intervals / qValue;
    const auto i = std::min(static_cast<std::size_t>(x), intervals - 1);
    return fWeight[i] + (x-i) * (fWeight[i+1]-fWeight[i]);
  }

 private:
  static constexpr std::size_t intervals = 4096;
  std::array<double, intervals + 1> fWeight;
};

Energies SampleTwoNu(const ElectronSpectrum& spectrum) {
  // Conditional sampling of density proportional to (Q-T1-T2)^5:
  // the marginal T1/Q is Beta(1,7), and T2/(Q-T1) is Beta(1,6).
  // Accept with the two Coulomb-corrected electron factors.
  for (;;) {
    const double remaining = qValue * std::pow(G4UniformRand(), 1./7.);
    const double first = qValue - remaining;
    const double second = remaining * (1. - std::pow(G4UniformRand(), 1./6.));
    if (G4UniformRand() < spectrum.Weight(first) * spectrum.Weight(second))
      return {first, second};
  }
}

Energies SampleZeroNu(const ElectronSpectrum& spectrum) {
  // Light Majorana mass mechanism: the electrons share a fixed total energy.
  for (;;) {
    const double first = qValue * G4UniformRand();
    const double second = qValue - first;
    if (G4UniformRand() < spectrum.Weight(first) * spectrum.Weight(second))
      return {first, second};
  }
}
}  // namespace

Mo100DoubleBetaDecayGenerator::Mo100DoubleBetaDecayGenerator(
    const GeometryManager& geometry, Mode mode) : fGeometry(geometry), fMode(mode) {}

G4ThreeVector Mo100DoubleBetaDecayGenerator::SamplePosition() {
  const auto generation = fGeometry.GetGeometryGeneration();
  if (fGeneration != generation) {
    std::vector<Source> sources;
    for (const auto& p : fGeometry.GetLMOPlacements())
      sources.push_back({DirectVolumeSampler(p.center, p.rotation), p.halfLengths});
    if (sources.empty())
      throw std::runtime_error("Mo-100 generation requires an enabled LMO crystal.");
    fSources = std::move(sources);
    fGeneration = generation;
  }
  // All crystals in the current array have identical Mo-100 inventories.
  const auto& source = fSources.at(static_cast<std::size_t>(G4UniformRand()*fSources.size()));
  return source.sampler.SampleBox(source.halfLengths);
}

void Mo100DoubleBetaDecayGenerator::Generate(G4Event* event) {
  const auto position = SamplePosition();
  static const ElectronSpectrum spectrum;  // Immutable after thread-safe initialization.
  const auto [t1,t2] = fMode == Mode::TwoNu ? SampleTwoNu(spectrum) : SampleZeroNu(spectrum);
  const double p1 = Momentum(t1);
  const double p2 = Momentum(t2);
  const double betaProduct = p1/(t1 + electron_mass_c2) * p2/(t2 + electron_mass_c2);
  const auto first = G4RandomDirection();
  G4ThreeVector second;
  // Conditional angular density proportional to 1 - beta1*beta2*cos(theta).
  do {
    second = G4RandomDirection();
  } while (G4UniformRand() * (1. + betaProduct) > 1. - betaProduct*first.dot(second));

  auto vertex = std::make_unique<G4PrimaryVertex>(position, 0.);
  for (const auto& p : {p1*first, p2*second})
    vertex->SetPrimary(new G4PrimaryParticle(G4Electron::ElectronDefinition(), p.x(), p.y(), p.z()));
  event->AddPrimaryVertex(vertex.release());
}
}  // namespace cupid
