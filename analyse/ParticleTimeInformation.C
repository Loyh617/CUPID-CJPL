/// Assign independent Poisson arrival times per LMO to positive incident-energy rows.
/// From the repository root: root -l -b -q 'analyse/ParticleTimeInformation.C("run_IncidentEnergy.root",3.0992)'
/// activityBq is the source parent activity; logName defaults to run.log beside the input.
/// ArriveTime/ms starts at zero separately in each detector. Shuffling discards event
/// and cross-detector timing correlations. This is an accidental-coincidence model.
#include "IncidentAnalysis.hh"

#include "TRandom3.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <random>
#include <sstream>
#include <vector>

namespace cupid_time {
Long64_t ProcessedEvents(const std::filesystem::path& input, const std::filesystem::path& log) {
  std::ifstream stream(log);
  cupid_analysis::Require(stream.is_open(), "Cannot open " + log.string());
  Long64_t count = 0;
  std::string line;
  while (std::getline(stream, line)) {
    if (line.rfind("CUPID_RUN_RESULT ", 0) != 0) continue;
    auto files = line.find(" file=");
    if (files == std::string::npos) continue;
    std::istringstream list(line.substr(files + 6));
    std::string path;
    bool matches = false;
    while (std::getline(list, path, ',')) {
      if (std::filesystem::path(path).filename() == input.filename()) matches = true;
    }
    if (!matches) continue;
    std::smatch value;
    cupid_analysis::Require(count == 0 && std::regex_search(line, value,
                            std::regex(" processed_events=([0-9]+)( |$)")),
                            "Ambiguous or invalid run result for " + input.string());
    count = std::stoll(value[1].str());
    cupid_analysis::Require(count > 0, "No processed parent events");
  }
  cupid_analysis::Require(count > 0, "No matching run result for " + input.string());
  return count;
}
}

void ParticleTimeInformation(const char* inputName, double activityBq,
                             const char* outputName = "", const char* logName = "",
                             unsigned int shuffleSeed = 20250828, unsigned int timeSeed = 12345) {
  using namespace cupid_analysis;
  Require(std::isfinite(activityBq) && activityBq > 0 && timeSeed > 0,
          "Activity must be positive and finite; time seed must be nonzero");
  const fs::path input(inputName);
  const fs::path log = *logName ? fs::path(logName) : input.parent_path() / "run.log";
  const auto parents = cupid_time::ProcessedEvents(input, log);
  std::mt19937 shuffle(shuffleSeed);
  TRandom3 clock(timeSeed);
  std::ostringstream settings;
  settings << std::setprecision(17) << "time_unit=ms activity_Bq=" << activityBq << " processed_events=" << parents
           << " shuffle_seed=" << shuffleSeed << " time_seed=" << timeSeed << " log=" << log.string();
  Process(input, outputName, "_timed", "", "timed", settings.str(),
          [&](TTree& tree, TTree& result) {
    Require(!tree.GetBranch("ArriveTime/ms"), "Input already contains arrival times");
    double timeMs = 0.;
    auto* branch = result.Branch("ArriveTime/ms", &timeMs, "ArriveTime/D");
    Require(branch, "Cannot add arrival time");
    const auto n = tree.GetEntries();
    if (n > 0) {
      const double meanMs = 1000. * parents / (activityBq * n);
      Require(std::isfinite(meanMs) && meanMs > 0, "Invalid mean arrival interval");
      std::vector<Long64_t> order(n);
      std::iota(order.begin(), order.end(), Long64_t{0});
      std::shuffle(order.begin(), order.end(), shuffle);
      for (Long64_t i = 0; i < n; ++i) {
        Read(tree, order[i]);
        if (i > 0) timeMs += clock.Exp(meanMs);
        Require(std::isfinite(timeMs), "Nonfinite arrival time");
        Keep(result);
      }
    }
    branch->ResetAddress();
  });
}
