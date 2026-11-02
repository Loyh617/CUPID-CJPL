/// \file include/output/RootOutput.hh
/// \brief Adapt typed records to the thread-local Geant4 ROOT analysis backend.
///
/// Select record types and configure detector names before OpenFile(). Each record
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
  // Inspect layout and count rows; recorder-specific physics checks belong in analysis/tests.
  static OutputSummary InspectFile(const std::string& fileName);
  RootOutput();
  ~RootOutput() override;
  RootOutput(const RootOutput&) = delete;
  RootOutput& operator=(const RootOutput&) = delete;
  RootOutput(RootOutput&&) = delete;
  RootOutput& operator=(RootOutput&&) = delete;

  std::string GetFileExtension() const override { return ".root"; }
  using VOutput::Book;
  using VOutput::Write;
  void Book(const RecordSchema& schema) override;
  void SetDetectorNames(const std::vector<G4String>& names) override;
  std::vector<std::string> GetFileNames(const std::string& baseName) const override;
  void OpenFile(const std::string& fileName) override;
  bool IsOpen() const override { return fOpen; }
  void Write(G4int runId, const RecordBatchView& records) override;
  void CloseFile() override;
  OutputSummary Inspect(const std::string& fileName) const override;

 private:
  struct Dataset {
    RecordSchema schema;
    std::map<G4String, G4int> ntuples;
  };
  G4int BookNtuple(const G4String& detectorName, const RecordSchema& schema);
  void BookSchemas();
  void RequireWritable(bool selected) const;
  void Fill(G4int ntupleId, G4int runId, const RecordBatchView& records, std::size_t row);
  G4GenericAnalysisManager* fAnalysis = nullptr;  // Owned by Geant4.
  std::vector<G4String> fDetectorNames;
  std::map<G4String, Dataset> fDatasets;
  bool fDetectorsSet = false;
  bool fSchemasBooked = false;
  bool fBackendOpen = false;
  bool fOpen = false;
  bool fHasOpened = false;
};
}  // namespace cupid
