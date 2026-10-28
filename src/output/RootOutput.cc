/// \file src/output/RootOutput.cc
/// \brief Book and fill Geant4 ntuples with native ROOT thread merging.
///
/// OutputManager selects this adapter. Master and workers book the same schemas;
/// only event-processing threads fill them. ROOT is used solely to inspect closed
/// files. Geant4 removes files with no recorded rows; run.log retains that result.

#include "output/RootOutput.hh"

#include "G4AnalysisManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4Threading.hh"

#include "TDirectory.h"
#include "TFile.h"
#include "TLeaf.h"
#include "TNamed.h"
#include "TTree.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace cupid {
namespace {
thread_local bool outputInUse = false;
void Require(bool success, const char* operation) {
  if (!success) throw std::runtime_error(std::string("RootOutput: Geant4 analysis failed to ") + operation);
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

G4int RootOutput::BookNtuple(const char* name, const char* title, bool incident) {
  if (fHasOpened) throw std::logic_error("RootOutput: book all schemas before the first OpenFile.");
  const auto id = fAnalysis->CreateNtuple(name, title);
  Require(id >= 0, "create ntuple");
  Require(fAnalysis->CreateNtupleIColumn(id, "RunID") == 0, "book RunID");
  Require(fAnalysis->CreateNtupleIColumn(id, "EventID") == 1, "book EventID");
  Require(fAnalysis->CreateNtupleDColumn(id, "Edep") == 2, "book Edep");
  Require(fAnalysis->CreateNtupleSColumn(id, "DetectorName") == 3, "book DetectorName");
  if (incident) Require(fAnalysis->CreateNtupleIColumn(id, "TrackID") == 4, "book TrackID");
  fAnalysis->FinishNtuple(id);
  return id;
}

void RootOutput::Book(const std::vector<EventEnergyRecord>&) {
  if (fEventNtuple < 0)
    fEventNtuple = BookNtuple("EventEnergy", "Energy per event and detector; Edep in keV; CUPID schema 2", false);
}
void RootOutput::Book(const std::vector<IncidentEnergyRecord>&) {
  if (fIncidentNtuple < 0)
    fIncidentNtuple = BookNtuple("IncidentEnergy", "Energy per incident root and detector; Edep in keV; CUPID schema 2", true);
}

void RootOutput::OpenFile(const std::string& fileName) {
  if (fOpen) throw std::logic_error("RootOutput: close the file before opening another.");
  if (fileName.empty() || std::filesystem::path(fileName).extension() != ".root")
    throw std::invalid_argument("RootOutput: provide a nonempty .root file name.");
  const auto parent = std::filesystem::absolute(fileName).parent_path();
  if (!std::filesystem::is_directory(parent)) throw std::runtime_error("RootOutput: output directory is missing.");
  // The master opens this same path before workers; only the file owner checks existence.
  if (!G4Threading::IsWorkerThread() && std::filesystem::exists(fileName))
    throw std::runtime_error("RootOutput: output already exists: " + fileName);
  Require(fAnalysis->SetFileName(fileName), "set file name");
  Require(fAnalysis->OpenFile(), "open file");
  fOpen = true;
  fHasOpened = true;
}

void RootOutput::RequireWritable(G4int ntupleId) const {
  if (!fOpen) throw std::logic_error("RootOutput: no open run.");
  if (ntupleId < 0) throw std::logic_error("RootOutput: record schema was not booked.");
  if (G4Threading::IsMultithreadedApplication() && !G4Threading::IsWorkerThread())
    throw std::logic_error("RootOutput: the MT master must not fill event ntuples.");
}

void RootOutput::Fill(G4int id, G4int runId, G4int eventId, G4double edep,
                       const G4String& detectorName, G4int trackId) {
  Require(fAnalysis->FillNtupleIColumn(id, 0, runId), "fill RunID");
  Require(fAnalysis->FillNtupleIColumn(id, 1, eventId), "fill EventID");
  Require(fAnalysis->FillNtupleDColumn(id, 2, edep / keV), "fill Edep");
  Require(fAnalysis->FillNtupleSColumn(id, 3, detectorName), "fill DetectorName");
  if (trackId > 0) Require(fAnalysis->FillNtupleIColumn(id, 4, trackId), "fill TrackID");
  Require(fAnalysis->AddNtupleRow(id), "append row");
}

void RootOutput::Write(G4int runId, const std::vector<EventEnergyRecord>& records) {
  WriteEventRecords(runId, records);
}
void RootOutput::Write(G4int runId, const std::vector<IncidentEnergyRecord>& records) {
  WriteIncidentRecords(runId, records);
}
void RootOutput::WriteEventRecords(G4int runId, const std::vector<EventEnergyRecord>& records) {
  RequireWritable(fEventNtuple);
  if (runId < 0) throw std::invalid_argument("RootOutput: negative RunID.");
  for (const auto& record : records) Validate(runId, record.eventId, record.edep, record.detectorName);
  for (const auto& record : records) Fill(fEventNtuple, runId, record.eventId, record.edep, record.detectorName);
}
void RootOutput::WriteIncidentRecords(G4int runId, const std::vector<IncidentEnergyRecord>& records) {
  RequireWritable(fIncidentNtuple);
  if (runId < 0) throw std::invalid_argument("RootOutput: negative RunID.");
  for (const auto& record : records) {
    Validate(runId, record.eventId, record.edep, record.detectorName);
    if (record.trackId <= 0) throw std::invalid_argument("RootOutput: non-positive TrackID.");
  }
  for (const auto& record : records)
    Fill(fIncidentNtuple, runId, record.eventId, record.edep, record.detectorName, record.trackId);
}

void RootOutput::CloseFile() {
  if (!fOpen) return;
  const bool written = fAnalysis->Write();
  const bool closed = fAnalysis->CloseFile();
  fOpen = false;
  Require(written && closed, "write and close file");
}

OutputSummary RootOutput::Inspect(const std::string& fileName) const {
  return InspectFile(fileName);
}
OutputSummary RootOutput::InspectFile(const std::string& fileName) {
  TDirectory::TContext context;
  std::unique_ptr<TFile> file(TFile::Open(fileName.c_str(), "READ"));
  if (!file || file->IsZombie() || file->TestBit(TFile::kRecovered))
    throw std::runtime_error("RootOutput: invalid file: " + fileName);
  // Accept historical direct-ROOT files as well as the native Geant4 layout.
  auto* schema = dynamic_cast<TNamed*>(file->Get("SchemaVersion"));
  auto* unit = dynamic_cast<TNamed*>(file->Get("EnergyUnit"));
  const bool legacy = schema && unit && std::string(schema->GetTitle()) == "1" &&
                      std::string(unit->GetTitle()) == "keV";
  OutputSummary result;
  bool found = false;
  for (const auto* name : {"EventEnergy", "IncidentEnergy"}) {
    TTree* tree = nullptr;
    file->GetObject(name, tree);
    if (!tree) continue;
    found = true;
    if (!legacy && std::string(tree->GetTitle()).find("Edep in keV; CUPID schema 2") == std::string::npos)
      throw std::runtime_error("RootOutput: unknown schema or energy unit.");
    for (const auto* column : {"RunID", "EventID", "Edep", "DetectorName"})
      if (!tree->GetBranch(column)) throw std::runtime_error("RootOutput: missing column.");
    if (std::string(name) == "EventEnergy") result.eventRows = tree->GetEntries();
    else {
      if (!tree->GetBranch("TrackID")) throw std::runtime_error("RootOutput: missing TrackID.");
      result.incidentRows = tree->GetEntries();
    }
  }
  if (!found && !legacy) throw std::runtime_error("RootOutput: no recognized data tables.");
  return result;
}
}  // namespace cupid
