#pragma once

#include "record/EventEnergyRecorder.hh"
#include "record/IncidentEnergyRecorder.hh"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class TFile;
class TTree;

namespace cupid {

struct RootFileSummary {
  std::int64_t eventRows = 0;
  std::int64_t incidentRows = 0;
};

// Use one instance and a distinct file per worker; never share an instance across threads.
class RootOutput final {
 public:
  // Merge closed files; validate the result before publishing it. Inputs are kept.
  static RootFileSummary MergeFiles(const std::string& outputName,
                                    const std::vector<std::string>& inputNames);
  static RootFileSummary InspectFile(const std::string& fileName);
  RootOutput();
  ~RootOutput();
  RootOutput(const RootOutput&) = delete;
  RootOutput& operator=(const RootOutput&) = delete;
  RootOutput(RootOutput&&) = delete;
  RootOutput& operator=(RootOutput&&) = delete;

  // Create a new file. Existing files are never overwritten.
  void OpenFile(const std::string& fileName);
  bool IsOpen() const;
  // Each call appends rows; an empty batch still creates the requested tree.
  void WriteEventRecords(G4int runId, const std::vector<EventEnergyRecord>& records);
  void WriteIncidentRecords(G4int runId, const std::vector<IncidentEnergyRecord>& records);
  // Call explicitly so that write failures can be handled by the caller.
  void CloseFile();

 private:
  void RequireOpen() const;
  TTree* CreateTree(const char* name, const char* title, bool incident);
  void Fill(TTree& tree);

  std::unique_ptr<TFile> fFile;
  // Trees are owned by fFile. Branch addresses refer to these persistent row buffers.
  TTree* fEventTree = nullptr;
  TTree* fIncidentTree = nullptr;
  G4int fRunId = 0;
  G4int fEventId = 0;
  G4int fTrackId = 0;
  G4double fEdep = 0.;
  std::string fDetectorName;
};

}  // namespace cupid
