/// \file include/output/RootOutput.hh
/// \brief Store typed energy records in ROOT and merge completed files.
///
/// OutputManager selects this VOutput backend. Write() overloads forward to
/// the standalone WriteEventRecords()/WriteIncidentRecords() interfaces.
/// Keep one writer per thread; input energies use Geant4 units, stored Edep
/// uses keV. MergeFiles() validates closed files and retains its inputs.

#pragma once

#include "output/VOutput.hh"

#include <memory>
#include <string>
#include <vector>

class TFile;
class TTree;

namespace cupid {

// Use one instance and a distinct file per worker; never share an instance across threads.
class RootOutput final : public VOutput {
 public:
  // Merge closed files; validate the result before publishing it. Inputs are kept.
  static OutputSummary MergeFiles(const std::string& outputName,
                                    const std::vector<std::string>& inputNames);
  static OutputSummary InspectFile(const std::string& fileName);
  RootOutput();
  ~RootOutput() override;
  RootOutput(const RootOutput&) = delete;
  RootOutput& operator=(const RootOutput&) = delete;
  RootOutput(RootOutput&&) = delete;
  RootOutput& operator=(RootOutput&&) = delete;

  std::string GetFileExtension() const override { return ".root"; }
  void Write(G4int runId, const std::vector<EventEnergyRecord>& records) override;
  void Write(G4int runId, const std::vector<IncidentEnergyRecord>& records) override;
  OutputSummary Merge(const std::string& outputName,
                      const std::vector<std::string>& inputNames) const override;
  OutputSummary Inspect(const std::string& fileName) const override;

  // Create a new file. Existing files are never overwritten.
  void OpenFile(const std::string& fileName) override;
  bool IsOpen() const override;
  // Each call appends rows; an empty batch still creates the requested tree.
  void WriteEventRecords(G4int runId, const std::vector<EventEnergyRecord>& records);
  void WriteIncidentRecords(G4int runId, const std::vector<IncidentEnergyRecord>& records);
  // Call explicitly so that write failures can be handled by the caller.
  void CloseFile() override;

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
