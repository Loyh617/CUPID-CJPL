/// \file include/output/OutputManager.hh
/// \brief Select one named output backend and forward typed record batches.
///
/// RunAction constructs one manager per thread, currently selecting RootOutput.
/// EventAction calls Write() with original recorder vectors. Selection is fixed
/// for the manager lifetime; no ROOT writer or open file is shared across workers.

#pragma once

#include "output/VOutput.hh"

#include <memory>
#include <string>
#include <vector>

namespace cupid {

class OutputManager final {
 public:
  explicit OutputManager(const std::string& name);
  const std::string& GetName() const { return fName; }
  std::string GetFileExtension() const;
  void OpenFile(const std::string& fileName);
  bool IsOpen() const;
  void Write(G4int runId, const std::vector<EventEnergyRecord>& records);
  void Write(G4int runId, const std::vector<IncidentEnergyRecord>& records);
  void CloseFile();
  OutputSummary MergeFiles(const std::string& outputName,
                           const std::vector<std::string>& inputNames) const;
  OutputSummary InspectFile(const std::string& fileName) const;

 private:
  const std::string fName;
  std::unique_ptr<VOutput> fOutput;
};

}  // namespace cupid
