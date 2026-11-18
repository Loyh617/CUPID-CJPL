/// Stage 3. Apply the same trigger threshold and Gaussian noise to every LMO.
/// root -l -b -q 'analyse/Trigger.C("run_IncidentEnergy_timed_pileup.root")'
/// thresholdKeV and sigmaKeV affect acceptance only. The retained Edep is unchanged.
#include "IncidentAnalysis.hh"

#include "TRandom3.h"

#include <cmath>
#include <iomanip>
#include <sstream>

void Trigger(const char* inputName, double thresholdKeV = 200., double sigmaKeV = 100.,
             unsigned int seed = 20250825, const char* outputName = "") {
  using namespace cupid_analysis;
  Require(std::isfinite(thresholdKeV) && std::isfinite(sigmaKeV) && sigmaKeV >= 0 && seed > 0,
          "Invalid trigger threshold, noise or seed");
  TRandom3 noise(seed);
  std::ostringstream settings;
  settings << std::setprecision(17) << "threshold_keV=" << thresholdKeV
           << " sigma_keV=" << sigmaKeV << " seed=" << seed;
  Process(inputName, outputName, "_trigger", "pileup", "trigger", settings.str(),
          [&](TTree& tree, TTree& result) {
    auto& energy = Column(tree, "Edep/keV", "Double_t");
    Column(tree, "ArriveTime/ms", "Double_t");
    for (Long64_t i = 0; i < tree.GetEntries(); ++i) {
      Read(tree, i);
      const double edep = energy.GetValue();
      Require(std::isfinite(edep) && edep > 0, "Invalid incident energy deposit");
      const double fluctuation = sigmaKeV > 0 ? noise.Gaus(0., sigmaKeV) : 0.;
      if (edep + fluctuation > thresholdKeV) Keep(result);
    }
  });
}
