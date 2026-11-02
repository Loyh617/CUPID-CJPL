/// \file src/output/RootOutput.cc
/// \brief Book and fill Geant4 ntuples with native ROOT thread merging.
///
/// OutputManager selects this adapter. Master and workers book the same schemas;
/// only event-processing threads fill them. ROOT is used solely to inspect closed
/// files. SetNtupleFileName routes each record type to its own file; detector
/// names become tree names. Geant4 handles merging and removal of empty files.

#include "output/RootOutput.hh"

#include "G4AnalysisManager.hh"
#include "G4Threading.hh"

#include "TDirectory.h"
#include "TFile.h"
#include "TKey.h"
#include "TLeaf.h"
#include "TNamed.h"
#include "TTree.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <type_traits>
#include <variant>

namespace cupid {
namespace {
thread_local bool outputInUse = false;
void Require(bool success, const char* operation) {
  if (!success) throw std::runtime_error(std::string("RootOutput: Geant4 analysis failed to ") + operation);
}
std::string RecordFileName(const std::string& baseName, const G4String& recordType) {
  const std::filesystem::path base(baseName);
  return (base.parent_path() / (base.stem().string() + "_" + recordType + ".root")).string();
}
}  // namespace

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
    if (!IsRecordIdentifier(name))
      throw std::invalid_argument("RootOutput: detector names must be ROOT identifiers: " + name);
  if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end())
    throw std::invalid_argument("RootOutput: duplicate detector name.");
  if ((fHasOpened || fSchemasBooked) && sorted != fDetectorNames)
    throw std::logic_error("RootOutput: detector catalog cannot change after booking.");
  fDetectorNames = std::move(sorted);
  fDetectorsSet = true;
}

G4int RootOutput::BookNtuple(const G4String& detectorName, const RecordSchema& schema) {
  const auto id = fAnalysis->CreateNtuple(detectorName, schema.Title());
  Require(id >= 0, "create ntuple");
  Require(fAnalysis->CreateNtupleIColumn(id, "RunID") == 0, "book RunID");
  G4int expected = 1;
  for (const auto& column : schema.columns) {
    G4int columnId = -1;
    switch (column.type) {
      case ColumnType::Int: columnId = fAnalysis->CreateNtupleIColumn(id, column.name); break;
      case ColumnType::Float: columnId = fAnalysis->CreateNtupleFColumn(id, column.name); break;
      case ColumnType::Double: columnId = fAnalysis->CreateNtupleDColumn(id, column.name); break;
      case ColumnType::String: columnId = fAnalysis->CreateNtupleSColumn(id, column.name); break;
    }
    Require(columnId == expected++, "book column");
  }
  fAnalysis->FinishNtuple(id);
  return id;
}

void RootOutput::Book(const RecordSchema& schema) {
  if (fHasOpened || fSchemasBooked) throw std::logic_error("RootOutput: select schemas before opening.");
  schema.Validate();
  const auto found = fDatasets.find(schema.name);
  if (found != fDatasets.end()) {
    if (!(found->second.schema == schema))
      throw std::invalid_argument("RootOutput: conflicting dataset schema: " + schema.name);
    return;
  }
  fDatasets.emplace(schema.name, Dataset{schema, {}});
}
void RootOutput::BookSchemas() {
  if (fSchemasBooked) return;
  // Sort both catalogs so registration order and empty workers cannot change IDs.
  for (auto& [name, dataset] : fDatasets)
    for (const auto& detector : fDetectorNames)
      dataset.ntuples.emplace(detector, BookNtuple(detector, dataset.schema));
  fSchemasBooked = true;
}
std::vector<std::string> RootOutput::GetFileNames(const std::string& baseName) const {
  std::vector<std::string> files;
  if (!fDetectorNames.empty())
    for (const auto& [name, dataset] : fDatasets) files.push_back(RecordFileName(baseName, name));
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
  if (!fDatasets.empty() && !fDetectorNames.empty()) {
    for (const auto& [name, dataset] : fDatasets)
      for (const auto& [detector, id] : dataset.ntuples)
        fAnalysis->SetNtupleFileName(id, RecordFileName(fileName, name));
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

void RootOutput::Fill(G4int id, G4int runId, const RecordBatchView& records, std::size_t row) {
  Require(fAnalysis->FillNtupleIColumn(id, 0, runId), "fill RunID");
  for (std::size_t column = 0; column < records.Schema().columns.size(); ++column) {
    const auto columnId = static_cast<G4int>(column + 1);
    const bool filled = std::visit([&](const auto& value) {
      using T = std::decay_t<decltype(value)>;
      if constexpr (std::is_same_v<T, G4int>) return fAnalysis->FillNtupleIColumn(id, columnId, value);
      else if constexpr (std::is_same_v<T, G4float>) return fAnalysis->FillNtupleFColumn(id, columnId, value);
      else if constexpr (std::is_same_v<T, G4double>) return fAnalysis->FillNtupleDColumn(id, columnId, value);
      else return fAnalysis->FillNtupleSColumn(id, columnId, value);
    }, records.Value(row, column));
    Require(filled, "fill column");
  }
  Require(fAnalysis->AddNtupleRow(id), "append row");
}

void RootOutput::Write(G4int runId, const RecordBatchView& records) {
  const auto found = fDatasets.find(records.Schema().name);
  RequireWritable(found != fDatasets.end());
  if (runId < 0) throw std::invalid_argument("RootOutput: negative RunID.");
  const auto& dataset = found->second;
  if (!(dataset.schema == records.Schema()))
    throw std::invalid_argument("RootOutput: batch schema differs from booked schema.");
  // Validate the complete batch before appending any rows. This is input checking,
  // not a transaction: an I/O failure after filling starts can still leave partial data.
  for (std::size_t row = 0; row < records.Size(); ++row) {
    records.ValidateRow(row);
    if (!dataset.ntuples.count(records.DetectorName(row)))
      throw std::invalid_argument("RootOutput: unregistered detector: " + records.DetectorName(row));
    for (std::size_t column = 0; column < dataset.schema.columns.size(); ++column)
      (void)records.Value(row, column);
  }
  for (std::size_t row = 0; row < records.Size(); ++row)
    Fill(dataset.ntuples.at(records.DetectorName(row)), runId, records, row);
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
    if (!IsRecordIdentifier(type) || !IsRecordIdentifier(tree->GetName()))
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
