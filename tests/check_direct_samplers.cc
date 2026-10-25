#include "generator/toolkit/sampler/DirectSurfaceSampler.hh"
#include "generator/toolkit/sampler/DirectVolumeSampler.hh"

#include "G4Box.hh"
#include "G4Orb.hh"
#include "G4PhysicalConstants.hh"
#include "G4SystemOfUnits.hh"
#include "G4Tubs.hh"
#include "Randomize.hh"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void Require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}
template<class F> void Reject(F operation) {
  bool rejected = false;
  try { operation(); }
  catch (const std::invalid_argument&) { rejected = true; }
  Require(rejected, "Invalid sampler parameters were accepted");
}

double AzimuthFraction(const G4ThreeVector& point) {
  auto angle = std::atan2(point.y(), point.x());
  if (angle < 0.) angle += twopi;
  return angle / twopi;
}

// Equal-measure cells test both marginal distributions and their correlations.
void CheckDistribution(const std::string& name,
                       const std::function<G4ThreeVector()>& draw,
                       const std::function<bool(const G4ThreeVector&)>& inSupport,
                       const std::function<std::array<double, 3>(const G4ThreeVector&)>& coordinates,
                       int dimensions, const G4ThreeVector& scale,
                       const G4ThreeVector& expectedMean, const G4ThreeVector& expectedSecond,
                       const G4ThreeVector& center, const G4RotationMatrix& rotation) {
  constexpr int samples = 60000;
  int cells = 1;
  for (int axis = 0; axis < dimensions; ++axis) cells *= 4;
  std::vector<int> counts(cells, 0);
  G4ThreeVector sum, second, cross;
  for (int sample = 0; sample < samples; ++sample) {
    const auto local = rotation.inverse() * (draw() - center);
    Require(inSupport(local), name + ": sample outside requested support");
    G4ThreeVector normalized;
    for (int axis = 0; axis < 3; ++axis) {
      normalized[axis] = local[axis] / scale[axis];
      sum[axis] += normalized[axis];
      second[axis] += normalized[axis] * normalized[axis];
    }
    cross += G4ThreeVector(normalized.x()*normalized.y(), normalized.x()*normalized.z(),
                           normalized.y()*normalized.z());
    const auto fractions = coordinates(local);
    int cell = 0;
    for (int axis = 0; axis < dimensions; ++axis) {
      Require(std::isfinite(fractions[axis]) && fractions[axis] >= -1.e-12 &&
              fractions[axis] <= 1.+1.e-12, name + ": invalid cell coordinate");
      cell = cell*4 + std::clamp(static_cast<int>(4.*fractions[axis]), 0, 3);
    }
    ++counts[cell];
  }
  for (int axis = 0; axis < 3; ++axis) {
    Require(std::abs(sum[axis]/samples - expectedMean[axis]) < 0.012, name + ": biased mean");
    Require(std::abs(second[axis]/samples - expectedSecond[axis]) < 0.012, name + ": incorrect second moment");
    Require(std::abs(cross[axis]/samples) < 0.012, name + ": correlated Cartesian components");
  }
  const double probability = 1./cells;
  const double expected = samples*probability;
  const double tolerance = 7.*std::sqrt(samples*probability*(1.-probability)) + 1.;
  for (int count : counts)
    Require(std::abs(count-expected) < tolerance, name + ": nonuniform equal-measure cells");
  std::cout << "PASS " << name << " support, moments, transform and " << cells << " equal-measure cells\n";
}
}

int main() {
  try {
    G4Random::setTheSeed(526934);
    const G4ThreeVector center{11.*mm, -23.*mm, 37.*mm};
    G4RotationMatrix rotation;
    rotation.rotateX(31.*deg);
    rotation.rotateY(-47.*deg);
    rotation.rotateZ(13.*deg);
    cupid::DirectVolumeSampler volume(center, rotation);
    cupid::DirectSurfaceSampler surface(center, rotation);
    const G4ThreeVector half{2.*mm, 3.*mm, 5.*mm};
    G4Box box("DirectTestBox", half.x(), half.y(), half.z());
    CheckDistribution("box", [&] { return volume.SampleBox(half); },
        [&](const auto& p) { return box.Inside(p) != kOutside; },
        [&](const auto& p) { return std::array<double,3>{(p.x()/half.x()+1.)/2.,
          (p.y()/half.y()+1.)/2., (p.z()/half.z()+1.)/2.}; },
        3, half, {}, {1./3.,1./3.,1./3.}, center, rotation);

    const double radius = 4.*mm, halfHeight = 6.*mm;
    G4Tubs cylinder("DirectTestCylinder", 0., radius, halfHeight, 0., twopi);
    CheckDistribution("cylinder", [&] { return volume.SampleCylinder(radius, halfHeight); },
        [&](const auto& p) { return cylinder.Inside(p) != kOutside; },
        [&](const auto& p) { return std::array<double,3>{p.perp2()/(radius*radius),
          AzimuthFraction(p), (p.z()/halfHeight+1.)/2.}; },
        3, {radius,radius,halfHeight}, {}, {1./4.,1./4.,1./3.}, center, rotation);

    const double sphereRadius = 7.*mm;
    const G4ThreeVector sphereScale{sphereRadius,sphereRadius,sphereRadius};
    G4Orb ball("DirectTestBall", sphereRadius);
    CheckDistribution("ball", [&] { return volume.SampleSphere(sphereRadius); },
        [&](const auto& p) { return ball.Inside(p) != kOutside; },
        [&](const auto& p) { return std::array<double,3>{std::pow(p.mag()/sphereRadius,3),
          (p.z()/p.mag()+1.)/2., AzimuthFraction(p)}; },
        3, sphereScale, {}, {1./5.,1./5.,1./5.}, center, rotation);

    CheckDistribution("rectangle", [&] { return surface.SampleRectangle(half.x(), half.y()); },
        [&](const auto& p) { return std::abs(p.z()) < 1.e-10*mm &&
          std::abs(p.x()) <= half.x()+1.e-10*mm && std::abs(p.y()) <= half.y()+1.e-10*mm; },
        [&](const auto& p) { return std::array<double,3>{(p.x()/half.x()+1.)/2.,
          (p.y()/half.y()+1.)/2., 0.}; },
        2, half, {}, {1./3.,1./3.,0.}, center, rotation);

    CheckDistribution("sphere surface", [&] { return surface.SampleSphere(sphereRadius); },
        [&](const auto& p) { return std::abs(p.mag()-sphereRadius) < 1.e-10*mm; },
        [&](const auto& p) { return std::array<double,3>{(p.z()/sphereRadius+1.)/2., AzimuthFraction(p), 0.}; },
        2, sphereScale, {}, {1./3.,1./3.,1./3.}, center, rotation);

    CheckDistribution("hemisphere surface", [&] { return surface.SampleHemisphere(sphereRadius); },
        [&](const auto& p) { return p.z() >= -1.e-10*mm && std::abs(p.mag()-sphereRadius) < 1.e-10*mm; },
        [&](const auto& p) { return std::array<double,3>{p.z()/sphereRadius, AzimuthFraction(p), 0.}; },
        2, sphereScale, {0.,0.,0.5}, {1./3.,1./3.,1./3.}, center, rotation);

    // A physical placement uses the supplied object rotation, not its inverse.
    G4RotationMatrix turn;
    turn.rotateY(90.*deg);
    cupid::DirectSurfaceSampler positiveX(center, turn);
    for (int i = 0; i < 100; ++i)
      Require(positiveX.SampleHemisphere(radius).x() >= center.x()-1.e-12*mm,
              "Hemisphere orientation is reversed");

    const auto sequence = [&] {
      return std::array<G4ThreeVector,6>{volume.SampleBox(half), volume.SampleCylinder(radius,halfHeight),
          volume.SampleSphere(radius), surface.SampleRectangle(half.x(),half.y()),
          surface.SampleSphere(radius), surface.SampleHemisphere(radius)};
    };
    G4Random::setTheSeed(987654);
    const auto expectedSequence = sequence();
    G4Random::setTheSeed(987654);
    Require(sequence() == expectedSequence, "Fixed-seed sequence is not reproducible");

    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const auto infinity = std::numeric_limits<double>::infinity();
    G4Random::setTheSeed(2468);
    const auto expectedRandom = G4UniformRand();
    G4Random::setTheSeed(2468);
    cupid::DirectVolumeSampler localVolume;
    cupid::DirectSurfaceSampler localSurface;
    for (double bad : {0., -1.*mm, nan, infinity}) {
      for (int axis = 0; axis < 3; ++axis) {
        auto invalidHalf = half;
        invalidHalf[axis] = bad;
        Reject([&] { localVolume.SampleBox(invalidHalf); });
      }
      Reject([&] { localVolume.SampleCylinder(bad, halfHeight); });
      Reject([&] { localVolume.SampleCylinder(radius, bad); });
      Reject([&] { localVolume.SampleSphere(bad); });
      Reject([&] { localSurface.SampleRectangle(bad, half.y()); });
      Reject([&] { localSurface.SampleRectangle(half.x(), bad); });
      Reject([&] { localSurface.SampleSphere(bad); });
      Reject([&] { localSurface.SampleHemisphere(bad); });
    }
    for (double bad : {nan, infinity}) {
      Reject([&] { cupid::DirectVolumeSampler invalid({0.,bad,0.}); });
      Reject([&] { cupid::DirectSurfaceSampler invalid({bad,0.,0.}); });
    }
    for (const auto& matrix : {
        G4RotationMatrix(G4Rep3x3(2.,0.,0., 0.,1.,0., 0.,0.,1.)),
        G4RotationMatrix(G4Rep3x3(-1.,0.,0., 0.,1.,0., 0.,0.,1.)),
        G4RotationMatrix(G4Rep3x3(nan,0.,0., 0.,1.,0., 0.,0.,1.))}) {
      Reject([&] { cupid::DirectVolumeSampler invalid({},matrix); });
      Reject([&] { cupid::DirectSurfaceSampler invalid({},matrix); });
    }
    Require(G4UniformRand() == expectedRandom, "Construction or rejected input consumed random numbers");
    std::cout << "PASS orientation, reproducibility and invalid-parameter handling\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
