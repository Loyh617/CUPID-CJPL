/// Stage 2. Reject all members of a pile-up cluster before applying a trigger.
/// root -l -b -q 'analyse/PileUp.C("run_IncidentEnergy_timed.root",300.)'
/// windowMs is the inclusive maximum neighbour separation in ms, independently per LMO.
#include "IncidentAnalysis.hh"

#include <cmath>
#include <iomanip>
#include <sstream>

void PileUp(const char* inputName, double windowMs = 300., const char* outputName = "") {
  using namespace cupid_analysis;
  Require(std::isfinite(windowMs) && windowMs >= 0, "Invalid pile-up window");
  std::ostringstream settings;
  settings << std::setprecision(17) << "window_ms=" << windowMs;
  Process(inputName, outputName, "_pileup", "timed", "pileup", settings.str(),
          [&](TTree& tree, TTree& result) {
    auto& time = Column(tree, "ArriveTime/ms", "Double_t");
    const auto n = tree.GetEntries();
    if (!n) return;
    // Read one time ahead; keep the current row only if both neighbours are distant.
    auto timeAt = [&](Long64_t i) {
      Require(time.GetBranch()->GetEntry(i) > 0, "Cannot read arrival time");
      const double t = time.GetValue();
      Require(std::isfinite(t) && t >= 0, "Invalid arrival time");
      return t;
    };
    double current = timeAt(0);
    bool nearPrevious = false;
    for (Long64_t i = 0; i < n; ++i) {
      const double next = i + 1 < n ? timeAt(i + 1) : current;
      Require(next >= current, "Arrival times are out of order");
      const bool nearNext = i + 1 < n && next - current <= windowMs;
      if (!nearPrevious && !nearNext) {
        Read(tree, i);
        Keep(result);
      }
      current = next;
      nearPrevious = nearNext;
    }
  });
}
