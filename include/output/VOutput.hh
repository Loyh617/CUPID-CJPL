/// \file include/output/VOutput.hh
/// \brief Define the typed data-output contract and completed-file row counts.
///
/// OutputManager owns a concrete implementation. Each Write() overload accepts
/// an original recorder result type; new types require an explicit output mapping.
/// Open and close files per run and per worker. Merge only closed worker files.

#pragma once

#include "record/EventEnergyRecorder.hh"
#include "record/IncidentEnergyRecorder.hh"

#include <cstdint>
#include <string>
#include <vector>

namespace cupid {

struct OutputSummary {
  std::int64_t eventRows = 0;
  std::int64_t incidentRows = 0;
};

class VOutput {
 public:
  virtual ~VOutput() = default;
  virtual std::string GetFileExtension() const = 0;
  virtual void OpenFile(const std::string& fileName) = 0;
  virtual bool IsOpen() const = 0;
  virtual void Write(G4int runId, const std::vector<EventEnergyRecord>& records) = 0;
  virtual void Write(G4int runId, const std::vector<IncidentEnergyRecord>& records) = 0;
  virtual void CloseFile() = 0;
  // Validate before publishing the final file; keep inputs for caller cleanup.
  virtual OutputSummary Merge(const std::string& outputName,
                              const std::vector<std::string>& inputNames) const = 0;
  virtual OutputSummary Inspect(const std::string& fileName) const = 0;
};

}  // namespace cupid
