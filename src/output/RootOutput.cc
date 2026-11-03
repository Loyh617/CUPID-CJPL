/// \file src/output/RootOutput.cc
/// \brief Book and fill Geant4 ntuples with native ROOT thread merging.
///
/// OutputManager selects this adapter. Master and workers book the same schemas;
/// only event-processing threads fill them. ROOT is used solely to inspect closed
/// files. SetNtupleFileName routes each record type to its own file; detector
/// names become tree names. Geant4 handles merging and removal of empty files.
/// Each record type has explicit booking and filling functions with matching column IDs.

#include "output/RootOutput.hh"

#include "G4AnalysisManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4Threading.hh"

#include "TDirectory.h"
#include "TFile.h"
#include "TKey.h"
#include "TLeaf.h"
#include "TNamed.h"
#include "TTree.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <regex>

namespace cupid {
namespace {
thread_local bool outputInUse = false;
void Require(bool success, const char* operation) {
  if (!success) throw std::runtime_error(std::string("RootOutput: Geant4 analysis failed to ") + operation);
}
std::string RecordFileName(const std::string& baseName, const char* recordType) {
  const std::filesystem::path base(baseName);
  return (base.parent_path() / (base.stem().string() + "_" + recordType + ".root")).string();
}
bool ValidDetectorName(const G4String& name) {
  static const std::regex pattern("[A-Za-z_][A-Za-z0-9_]*");
  return std::regex_match(name, pattern);
}
void Validate(G4int runId, G4int eventId, G4double edep, const G4String& detectorName) {
  if (runId < 0 || eventId < 0 || detectorName.empty() || !std::isfinite(edep) || edep < 0. ||
      !std::isfinite(edep / keV))
    throw std::invalid_argument("RootOutput: invalid energy record.");
}
}

RootOutput::RootOutput() {
  if (outputInUse) throw std::logic_error("RootOutput: only one analysis adapter per thread is allowed.");
  fAnalysis = G4AnalysisManager::Instance();
  if (fAnalysis->IsOpenFile() || fAnalysis->GetNofNtuples() != 0)
    throw std::logic_error("RootOutput: Geant4 analysis is already in use.");
  fAnalysis->SetDefaultFileType("root");
  fAnalysis->SetNtupleMerging(G4Threading::IsMultithreadedApplication());
  // Separate branches while preserving the association of all columns in a row.
  fAnalysis->SetNtupleRowWise(false, true);
  Require(fAnalysis->GetFirstNtupleId() == 0, "use zero-based ntuple IDs");
  Require(fAnalysis->GetFirstNtupleColumnId() == 0, "use zero-based column IDs");
  outputInUse = true;
}

RootOutput::~RootOutput() {
  if (G4AnalysisManager::IsInstance()) {
    try { CloseFile(); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; }
    fAnalysis->Clear();
  }
  outputInUse = false;
}

void RootOutput::SetDetectorNames(const std::vector<G4String>& names) {
  auto sorted = names;
  std::sort(sorted.begin(), sorted.end());
  for (const auto& name : sorted)
    if (!ValidDetectorName(name))
      throw std::invalid_argument("RootOutput: detector names must be ROOT identifiers: " + name);
  if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end())
    throw std::invalid_argument("RootOutput: duplicate detector name.");
  if ((fHasOpened || fSchemasBooked) && sorted != fDetectorNames)
    throw std::logic_error("RootOutput: detector catalog cannot change after booking.");
  fDetectorNames = std::move(sorted);
  fDetectorsSet = true;
}

// Column IDs below must match the corresponding Fill* function and readback tests.
G4int RootOutput::BookEventNtuple(const G4String& detectorName) {
  const auto id = fAnalysis->CreateNtuple(detectorName, "EventEnergy; Edep in keV; CUPID schema 4");
  Require(id >= 0, "create event ntuple");
  Require(fAnalysis->CreateNtupleIColumn(id, "RunID") == 0, "book RunID");
  Require(fAnalysis->CreateNtupleIColumn(id, "EventID") == 1, "book EventID");
  Require(fAnalysis->CreateNtupleDColumn(id, "Edep") == 2, "book Edep");
  fAnalysis->FinishNtuple(id);
  return id;
}

G4int RootOutput::BookIncidentNtuple(const G4String& detectorName) {
  const auto id = fAnalysis->CreateNtuple(detectorName, "IncidentEnergy; Edep in keV; CUPID schema 4");
  Require(id >= 0, "create incident ntuple");
  Require(fAnalysis->CreateNtupleIColumn(id, "RunID") == 0, "book RunID");
  Require(fAnalysis->CreateNtupleIColumn(id, "EventID") == 1, "book EventID");
  Require(fAnalysis->CreateNtupleIColumn(id, "TrackID") == 2, "book TrackID");
  Require(fAnalysis->CreateNtupleIColumn(id, "PDG") == 3, "book PDG");
  Require(fAnalysis->CreateNtupleDColumn(id, "Edep") == 4, "book Edep");
  fAnalysis->FinishNtuple(id);
  return id;
}

void RootOutput::BookEventEnergy() {
  if (fHasOpened || fSchemasBooked) throw std::logic_error("RootOutput: select schemas before opening.");
  fEvents = true;
}
void RootOutput::BookIncidentEnergy() {
  if (fHasOpened || fSchemasBooked) throw std::logic_error("RootOutput: select schemas before opening.");
  fIncidents = true;
}
void RootOutput::BookSchemas() {
  if (fSchemasBooked) return;
  // A stable type/name order keeps ntuple IDs identical on master and workers,
  // including workers with no events and detectors with no deposited energy.
  for (const auto& name : fDetectorNames) {
    if (fEvents) fEventNtuples.emplace(name, BookEventNtuple(name));
    if (fIncidents) fIncidentNtuples.emplace(name, BookIncidentNtuple(name));
  }
  fSchemasBooked = true;
}
std::vector<std::string> RootOutput::GetFileNames(const std::string& baseName) const {
  std::vector<std::string> files;
  if (fDetectorNames.empty()) return files;
  if (fEvents) files.push_back(RecordFileName(baseName, "EventEnergy"));
  if (fIncidents) files.push_back(RecordFileName(baseName, "IncidentEnergy"));
  return files;
}

void RootOutput::OpenFile(const std::string& fileName) {
  if (fOpen) throw std::logic_error("RootOutput: close the run before opening another.");
  if (!fDetectorsSet) throw std::logic_error("RootOutput: configure detector names before opening.");
  if (fileName.empty() || std::filesystem::path(fileName).extension() != ".root")
    throw std::invalid_argument("RootOutput: provide a nonempty .root base name.");
  const auto parent = std::filesystem::absolute(fileName).parent_path();
  if (!std::filesystem::is_directory(parent)) throw std::runtime_error("RootOutput: output directory is missing.");
  if (!G4Threading::IsWorkerThread()) {
    auto files = GetFileNames(fileName);
    files.push_back(fileName);
    for (const auto& path : files)
      if (std::filesystem::exists(path))
        throw std::runtime_error("RootOutput: output already exists: " + path);
  }
  BookSchemas();
  if (!fEventNtuples.empty() || !fIncidentNtuples.empty()) {
    for (const auto& [name, id] : fEventNtuples)
      fAnalysis->SetNtupleFileName(id, RecordFileName(fileName, "EventEnergy"));
    for (const auto& [name, id] : fIncidentNtuples)
      fAnalysis->SetNtupleFileName(id, RecordFileName(fileName, "IncidentEnergy"));
    Require(fAnalysis->SetFileName(fileName), "set run base name");
    Require(fAnalysis->OpenFile(), "open files");
    fBackendOpen = true;
  }
  fOpen = true;
  fHasOpened = true;
}

void RootOutput::RequireWritable(bool selected) const {
  if (!fOpen) throw std::logic_error("RootOutput: no open run.");
  if (!selected) throw std::logic_error("RootOutput: record schema was not booked.");
  if (G4Threading::IsMultithreadedApplication() && !G4Threading::IsWorkerThread())
    throw std::logic_error("RootOutput: the MT master must not fill event ntuples.");
}

void RootOutput::FillEventEnergy(G4int id, G4int runId, const EventEnergyRecord& record) {
  Require(fAnalysis->FillNtupleIColumn(id, 0, runId), "fill RunID");
  Require(fAnalysis->FillNtupleIColumn(id, 1, record.eventId), "fill EventID");
  Require(fAnalysis->FillNtupleDColumn(id, 2, record.edep / keV), "fill Edep");
  Require(fAnalysis->AddNtupleRow(id), "append event row");
}

void RootOutput::FillIncidentEnergy(G4int id, G4int runId, const IncidentEnergyRecord& record) {
  Require(fAnalysis->FillNtupleIColumn(id, 0, runId), "fill RunID");
  Require(fAnalysis->FillNtupleIColumn(id, 1, record.eventId), "fill EventID");
  Require(fAnalysis->FillNtupleIColumn(id, 2, record.trackId), "fill TrackID");
  Require(fAnalysis->FillNtupleIColumn(id, 3, record.pdg), "fill PDG");
  Require(fAnalysis->FillNtupleDColumn(id, 4, record.edep / keV), "fill Edep");
  Require(fAnalysis->AddNtupleRow(id), "append incident row");
}

// Validate the whole input vector before appending rows; an I/O failure is still
// not transactional. Energies remain in internal units until Fill* converts to keV.
void RootOutput::Write(G4int runId, const std::vector<EventEnergyRecord>& records) {
  RequireWritable(fEvents);
  if (runId < 0) throw std::invalid_argument("RootOutput: negative RunID.");
  for (const auto& record : records) {
    Validate(runId, record.eventId, record.edep, record.detectorName);
    if (!fEventNtuples.count(record.detectorName))
      throw std::invalid_argument("RootOutput: unregistered detector: " + record.detectorName);
  }
  for (const auto& record : records)
    FillEventEnergy(fEventNtuples.at(record.detectorName), runId, record);
}
void RootOutput::Write(G4int runId, const std::vector<IncidentEnergyRecord>& records) {
  RequireWritable(fIncidents);
  if (runId < 0) throw std::invalid_argument("RootOutput: negative RunID.");
  for (const auto& record : records) {
    Validate(runId, record.eventId, record.edep, record.detectorName);
    if (record.trackId <= 0) throw std::invalid_argument("RootOutput: non-positive TrackID.");
    if (!fIncidentNtuples.count(record.detectorName))
      throw std::invalid_argument("RootOutput: unregistered detector: " + record.detectorName);
  }
  for (const auto& record : records)
    FillIncidentEnergy(fIncidentNtuples.at(record.detectorName), runId, record);
}

void RootOutput::CloseFile() {
  if (!fOpen) return;
  bool success = true;
  if (fBackendOpen) {
    const bool written = fAnalysis->Write();
    const bool closed = fAnalysis->CloseFile();
    success = written && closed;
  }
  fOpen = false;
  fBackendOpen = false;
  Require(success, "write and close files");
}

OutputSummary RootOutput::Inspect(const std::string& fileName) const {
  return InspectFile(fileName);
}
OutputSummary RootOutput::InspectFile(const std::string& fileName) {
  TDirectory::TContext context;
  std::unique_ptr<TFile> file(TFile::Open(fileName.c_str(), "READ"));
  if (!file || file->IsZombie() || file->TestBit(TFile::kRecovered))
    throw std::runtime_error("RootOutput: invalid file: " + fileName);
  OutputSummary result;
  auto* schema = dynamic_cast<TNamed*>(file->Get("SchemaVersion"));
  auto* unit = dynamic_cast<TNamed*>(file->Get("EnergyUnit"));
  const bool legacy = schema && unit && std::string(schema->GetTitle()) == "1" &&
                      std::string(unit->GetTitle()) == "keV";
  std::string splitType;
  std::string splitTitle;
  std::vector<std::pair<std::string, std::string>> splitColumns;
  bool found = false;
  TIter next(file->GetListOfKeys());
  while (auto* key = dynamic_cast<TKey*>(next())) {
    if (std::string(key->GetClassName()) != "TTree") continue;
    // Inspect the latest tree cycle once; historical metadata can have cycles.
    if (file->GetKey(key->GetName())->GetCycle() != key->GetCycle()) continue;
    auto* tree = dynamic_cast<TTree*>(file->Get(key->GetName()));
    if (!tree) throw std::runtime_error("RootOutput: unreadable tree.");
    const std::string title = tree->GetTitle();
    const std::string suffix = "; CUPID schema 4";
    const bool split = title.size() >= suffix.size() &&
                       title.compare(title.size() - suffix.size(), suffix.size(), suffix) == 0;
    const std::string type = split ? title.substr(0, title.find(';')) : tree->GetName();
    if (!ValidDetectorName(type) || !ValidDetectorName(tree->GetName()))
      throw std::runtime_error("RootOutput: invalid dataset or detector name.");
    if (!split && !legacy && title.find("Edep in keV; CUPID schema 2") == std::string::npos &&
        title.find("Edep in keV; CUPID schema 3") == std::string::npos)
      throw std::runtime_error("RootOutput: unknown schema.");
    const auto* run = tree->GetLeaf("RunID");
    if (!run || std::string(run->GetTypeName()) != "Int_t")
      throw std::runtime_error("RootOutput: missing or invalid RunID.");
    if (split) {
      std::vector<std::pair<std::string, std::string>> columns;
      TIter branches(tree->GetListOfBranches());
      while (auto* branch = branches()) {
        const auto* leaf = tree->GetLeaf(branch->GetName());
        if (!leaf) throw std::runtime_error("RootOutput: unsupported column representation.");
        const std::string leafType = leaf->GetTypeName();
        if (leafType != "Int_t" && leafType != "Float_t" && leafType != "Double_t" && leafType != "Char_t")
          throw std::runtime_error("RootOutput: unsupported scalar column type.");
        columns.emplace_back(branch->GetName(), leafType);
      }
      if (columns.size() < 2 || columns.front().first != "RunID" ||
          (found && (splitType != type || splitTitle != title || splitColumns != columns)))
        throw std::runtime_error("RootOutput: inconsistent detector tree schemas.");
      splitType = type;
      splitTitle = title;
      splitColumns = std::move(columns);
    } else if (!splitType.empty()) throw std::runtime_error("RootOutput: mixed file layouts.");
    result.rows[type] += tree->GetEntries();
    found = true;
  }
  if (!found && !legacy) throw std::runtime_error("RootOutput: no recognized data tables.");
  return result;
}
}  // namespace cupid
