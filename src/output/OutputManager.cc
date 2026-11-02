/// \file src/output/OutputManager.cc
/// \brief Create the selected output backend and forward its lifecycle and data.
///
/// Register backend factories here. Name lookup happens once at construction;
/// Write() dispatches by the vector's C++ type, not a recorder-name comparison.

#include "output/OutputManager.hh"
#include "output/RootOutput.hh"

#include <map>
#include <stdexcept>

namespace cupid {

OutputManager::OutputManager(const std::string& name) : fName(name) {
  using Factory = std::unique_ptr<VOutput> (*)();
  static const std::map<std::string, Factory> factories{
      {"RootOutput", []() -> std::unique_ptr<VOutput> { return std::make_unique<RootOutput>(); }}};
  const auto found = factories.find(name);
  if (found == factories.end())
    throw std::invalid_argument("Unknown output module: " + name);
  fOutput = found->second();
}

std::string OutputManager::GetFileExtension() const { return fOutput->GetFileExtension(); }
void OutputManager::BookEventEnergy() { fOutput->BookEventEnergy(); }
void OutputManager::BookIncidentEnergy() { fOutput->BookIncidentEnergy(); }
void OutputManager::SetDetectorNames(const std::vector<G4String>& names) { fOutput->SetDetectorNames(names); }
std::vector<std::string> OutputManager::GetFileNames(const std::string& baseName) const {
  return fOutput->GetFileNames(baseName);
}
void OutputManager::OpenFile(const std::string& fileName) { fOutput->OpenFile(fileName); }
bool OutputManager::IsOpen() const { return fOutput->IsOpen(); }
void OutputManager::Write(G4int runId, const std::vector<EventEnergyRecord>& records) {
  fOutput->Write(runId, records);
}
void OutputManager::Write(G4int runId, const std::vector<IncidentEnergyRecord>& records) {
  fOutput->Write(runId, records);
}
void OutputManager::CloseFile() { fOutput->CloseFile(); }
OutputSummary OutputManager::InspectFile(const std::string& fileName) const {
  if (IsOpen()) throw std::logic_error("OutputManager: close the file before inspection.");
  return fOutput->Inspect(fileName);
}

}  // namespace cupid
