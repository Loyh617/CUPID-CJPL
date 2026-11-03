/// \file include/output/RunLog.hh
/// \brief Capture console output in the active run's log while retaining terminal output.
///
/// main() owns the capture before workers start and until after shutdown.
/// Master RunAction calls BeginRun/EndRun to select each run.log. Initialization
/// and between-run context is retained in memory and copied into later logs;
/// event output is written only to its own run. RecordMacro preserves input text.

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace cupid {
// Capture application console output while retaining the normal terminal display.
// Construct before worker threads and destroy after they have joined.
class RunLog final {
 public:
  RunLog();
  // Called by the master outside event processing; streams stay installed once.
  void BeginRun(const std::string& fileName);
  void EndRun();
  ~RunLog();
  RunLog(const RunLog&) = delete;
  RunLog& operator=(const RunLog&) = delete;
  void RecordMacro(const std::string& fileName, const std::vector<std::string>& searchPaths);
  void Check() const;

 private:
  class State;
  std::unique_ptr<State> fState;
};
}  // namespace cupid
