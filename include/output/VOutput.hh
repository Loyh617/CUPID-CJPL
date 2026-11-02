/// \file include/output/VOutput.hh
/// \brief Define explicit energy-table booking and typed output functions.
///
/// RunAction selects tables before opening files; EventAction passes the original
/// record vectors by const reference. Every backend implements both Write overloads.
/// Table selections and detector catalogs must agree on master and workers.

#pragma once

#include "record/EventEnergyRecorder.hh"
#include "record/IncidentEnergyRecorder.hh"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace cupid {
struct OutputSummary {
  std::map<G4String, std::int64_t> rows;
  std::int64_t Count(const G4String& name) const {
    const auto found = rows.find(name);
    return found == rows.end() ? 0 : found->second;
  }
};

class VOutput {
 public:
  virtual ~VOutput() = default;
  virtual std::string GetFileExtension() const = 0;
  virtual void BookEventEnergy() = 0;
  virtual void BookIncidentEnergy() = 0;
  virtual void SetDetectorNames(const std::vector<G4String>& names) = 0;
  virtual std::vector<std::string> GetFileNames(const std::string& baseName) const = 0;
  virtual void OpenFile(const std::string& fileName) = 0;
  virtual bool IsOpen() const = 0;
  virtual void Write(G4int runId, const std::vector<EventEnergyRecord>& records) = 0;
  virtual void Write(G4int runId, const std::vector<IncidentEnergyRecord>& records) = 0;
  virtual void CloseFile() = 0;
  virtual OutputSummary Inspect(const std::string& fileName) const = 0;
};
}  // namespace cupid
