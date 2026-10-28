/// \file include/output/RunLog.hh
/// \brief Declaration: Capture console output and run provenance in one job-level text log.
///
/// main() constructs RunLog before workers start and keeps it alive until
/// they finish. RecordMacro() saves macro text and resolvable static includes;
/// Check() reports write failures. RunAction summaries use the captured
/// console streams; this class does not write ROOT event data.

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace cupid {
// Capture application console output while retaining the normal terminal display.
// Construct before worker threads and destroy after they have joined.
class RunLog final {
 public:
  explicit RunLog(const std::string& fileName);
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
