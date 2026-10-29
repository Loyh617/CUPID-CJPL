/// \file include/output/RootOutput.hh
/// \brief Adapt typed records to the thread-local Geant4 ROOT analysis backend.
///
/// Book selected schemas identically on master and workers before OpenFile().
/// Write() fills worker ntuples; CloseFile() calls native Write/CloseFile on every
/// thread. Geant4 owns the analysis manager and merges the ROOT ntuples itself.

#pragma once

#include "output/VOutput.hh"

#include <string>
#include <vector>

class G4GenericAnalysisManager;

namespace cupid {
class RootOutput final : public VOutput {
 public:
  static OutputSummary InspectFile(const std::string& fileName);
  RootOutput();
  ~RootOutput() override;
  RootOutput(const RootOutput&) = delete;
  RootOutput& operator=(const RootOutput&) = delete;
  RootOutput(RootOutput&&) = delete;
  RootOutput& operator=(RootOutput&&) = delete;

  std::string GetFileExtension() const override { return ".root"; }
  void Book(const std::vector<EventEnergyRecord>&) override;
  void Book(const std::vector<IncidentEnergyRecord>&) override;
  void OpenFile(const std::string& fileName) override;
  bool IsOpen() const override { return fOpen; }
  void Write(G4int runId, const std::vector<EventEnergyRecord>& records) override;
  void Write(G4int runId, const std::vector<IncidentEnergyRecord>& records) override;
  void WriteEventRecords(G4int runId, const std::vector<EventEnergyRecord>& records);
  void WriteIncidentRecords(G4int runId, const std::vector<IncidentEnergyRecord>& records);
  void CloseFile() override;
  OutputSummary Inspect(const std::string& fileName) const override;

 private:
  G4int BookNtuple(const char* name, const char* title, bool incident);
  void RequireWritable(G4int ntupleId) const;
  void Fill(G4int ntupleId, G4int runId, G4int eventId, G4double edep,
            const G4String& detectorName, G4int trackId = 0, G4int pdg = 0);
  G4GenericAnalysisManager* fAnalysis = nullptr;  // Owned by Geant4.
  G4int fEventNtuple = -1;
  G4int fIncidentNtuple = -1;
  bool fOpen = false;
  bool fHasOpened = false;
};
}  // namespace cupid
