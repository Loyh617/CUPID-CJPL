/// \file include/output/VOutput.hh
/// \brief Define the typed data-output contract and completed-file row counts.
///
/// OutputManager owns a concrete implementation. Each Write() overload accepts
/// an original recorder result type; new types require an explicit output mapping.
/// Select matching schemas and detector catalogs on master and workers; the
/// backend owns per-recorder file routing and merging. Inspect reads one file.

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
  virtual void Book(const std::vector<EventEnergyRecord>& records) = 0;
  virtual void Book(const std::vector<IncidentEnergyRecord>& records) = 0;
  // Configure the complete detector catalog identically on all threads before opening.
  virtual void SetDetectorNames(const std::vector<G4String>& names) = 0;
  // OpenFile takes a run base name; implementations may route records to several files.
  virtual std::vector<std::string> GetFileNames(const std::string& baseName) const = 0;
  virtual void OpenFile(const std::string& fileName) = 0;
  virtual bool IsOpen() const = 0;
  virtual void Write(G4int runId, const std::vector<EventEnergyRecord>& records) = 0;
  virtual void Write(G4int runId, const std::vector<IncidentEnergyRecord>& records) = 0;
  virtual void CloseFile() = 0;
  virtual OutputSummary Inspect(const std::string& fileName) const = 0;
};

}  // namespace cupid
