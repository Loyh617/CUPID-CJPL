/// \file include/output/RootOutput.hh
/// \brief Adapt typed records to the thread-local Geant4 ROOT analysis backend.
///
/// Call BookEventEnergy()/BookIncidentEnergy() and SetDetectorNames() before
/// OpenFile(). Booking/filling functions explicitly list each record's columns. Each record
/// type gets its own file, with one identically booked ntuple per detector.
/// Write() fills worker ntuples; CloseFile() calls native Write/CloseFile on every
/// thread. Geant4 owns the analysis manager and merges the ROOT ntuples itself.

#pragma once

#include "output/VOutput.hh"

#include <map>
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
  void BookEventEnergy() override;
  void BookIncidentEnergy() override;
  void SetDetectorNames(const std::vector<G4String>& names) override;
  std::vector<std::string> GetFileNames(const std::string& baseName) const override;
  void OpenFile(const std::string& fileName) override;
  bool IsOpen() const override { return fOpen; }
  void Write(G4int runId, const std::vector<EventEnergyRecord>& records) override;
  void Write(G4int runId, const std::vector<IncidentEnergyRecord>& records) override;
  void CloseFile() override;
  OutputSummary Inspect(const std::string& fileName) const override;

 private:
  G4int BookEventNtuple(const G4String& detectorName);
  G4int BookIncidentNtuple(const G4String& detectorName);
  void BookSchemas();
  void RequireWritable(bool selected) const;
  void FillEventEnergy(G4int ntupleId, G4int runId, const EventEnergyRecord& record);
  void FillIncidentEnergy(G4int ntupleId, G4int runId, const IncidentEnergyRecord& record);
  G4GenericAnalysisManager* fAnalysis = nullptr;  // Owned by Geant4.
  std::vector<G4String> fDetectorNames;
  std::map<G4String, G4int> fEventNtuples;
  std::map<G4String, G4int> fIncidentNtuples;
  bool fEvents = false;
  bool fIncidents = false;
  bool fDetectorsSet = false;
  bool fSchemasBooked = false;
  bool fBackendOpen = false;
  bool fOpen = false;
  bool fHasOpened = false;
};
}  // namespace cupid
