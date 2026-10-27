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
