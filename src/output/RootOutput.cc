/// \file src/output/RootOutput.cc
/// \brief Store typed energy records in ROOT and merge completed files.
///
/// OutputManager selects this VOutput backend. Write() overloads forward to
/// the standalone WriteEventRecords()/WriteIncidentRecords() interfaces.
/// Keep one writer per thread; input energies use Geant4 units, stored Edep
/// uses keV. MergeFiles() validates closed files and retains its inputs.

#include "output/RootOutput.hh"

#include "G4SystemOfUnits.hh"

#include "TDirectory.h"
#include "TFile.h"
#include "TFileMerger.h"
#include "TNamed.h"
#include "TROOT.h"
#include "TTree.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <set>
#include <limits>
#include <mutex>
#include <stdexcept>

namespace cupid {
namespace {
void Validate(G4int runId, G4int eventId, G4double edep, const G4String& detectorName) {
  if (runId < 0 || eventId < 0 || detectorName.empty() || !std::isfinite(edep) || edep < 0. ||
      !std::isfinite(edep / keV))
    throw std::invalid_argument("RootOutput: invalid energy record.");
}
}

void RootOutput::Write(G4int runId, const std::vector<EventEnergyRecord>& records) {
  WriteEventRecords(runId, records);
}
void RootOutput::Write(G4int runId, const std::vector<IncidentEnergyRecord>& records) {
  WriteIncidentRecords(runId, records);
}
OutputSummary RootOutput::Merge(const std::string& outputName,
                                const std::vector<std::string>& inputNames) const {
  return MergeFiles(outputName, inputNames);
}
OutputSummary RootOutput::Inspect(const std::string& fileName) const {
  return InspectFile(fileName);
}

OutputSummary RootOutput::InspectFile(const std::string& fileName) {
  TDirectory::TContext context;
  std::unique_ptr<TFile> file(TFile::Open(fileName.c_str(), "READ"));
  if (!file || file->IsZombie() || file->TestBit(TFile::kRecovered))
    throw std::runtime_error("RootOutput: invalid file: " + fileName);
  TNamed* schema = nullptr;
  TNamed* unit = nullptr;
  file->GetObject("SchemaVersion", schema);
  file->GetObject("EnergyUnit", unit);
  if (!schema || !unit || std::string(schema->GetTitle()) != "1" || std::string(unit->GetTitle()) != "keV")
    throw std::runtime_error("RootOutput: incompatible metadata: " + fileName);
  TTree* events = nullptr;
  TTree* incidents = nullptr;
  file->GetObject("EventEnergy", events);
  file->GetObject("IncidentEnergy", incidents);
  return {events ? events->GetEntries() : 0, incidents ? incidents->GetEntries() : 0};
}

OutputSummary RootOutput::MergeFiles(const std::string& outputName,
                                       const std::vector<std::string>& inputNames) {
  if (inputNames.empty()) throw std::invalid_argument("RootOutput: no files to merge.");
  const auto output = std::filesystem::absolute(outputName).lexically_normal();
  const auto partial = output.string() + ".partial";
  if (std::filesystem::exists(output) || std::filesystem::exists(partial))
    throw std::runtime_error("RootOutput: merge destination already exists: " + output.string());
  OutputSummary expected;
  std::set<std::filesystem::path> inputs;
  for (const auto& name : inputNames) {
    const auto input = std::filesystem::canonical(name);
    if (input == output || !inputs.insert(input).second)
      throw std::invalid_argument("RootOutput: duplicate input or output used as input.");
    const auto counts = InspectFile(name);
    expected.eventRows += counts.eventRows;
    expected.incidentRows += counts.incidentRows;
  }
  {
    TDirectory::TContext context;
    TFileMerger merger(false);
    merger.SetPrintLevel(-1);
    if (!merger.OutputFile(partial.c_str(), "CREATE"))
      throw std::runtime_error("RootOutput: cannot create merge output.");
    for (const auto& name : inputNames)
      if (!merger.AddFile(name.c_str(), false))
        throw std::runtime_error("RootOutput: cannot add merge input: " + name);
    if (!merger.Merge()) throw std::runtime_error("RootOutput: file merge failed; inputs retained.");
    if (merger.GetOutputFile() && merger.GetOutputFile()->TestBit(TFile::kWriteError))
      throw std::runtime_error("RootOutput: merged file has a write error; inputs retained.");
    merger.CloseOutputFile();
  }
  const auto actual = InspectFile(partial);
  if (actual.eventRows != expected.eventRows || actual.incidentRows != expected.incidentRows)
    throw std::runtime_error("RootOutput: merged row counts differ; inputs retained.");
  std::filesystem::rename(partial, output);
  return actual;
}

RootOutput::RootOutput() {
  static std::once_flag initialized;
  std::call_once(initialized, [] {
    ROOT::EnableThreadSafety();
    // ROOT's automatic rollover can replace/delete the TFile owned by this module.
    // Keep a single file; file rotation must be arranged explicitly by the caller.
    TTree::SetMaxTreeSize(std::numeric_limits<Long64_t>::max());
  });
}

RootOutput::~RootOutput() {
  try {
    CloseFile();
  } catch (const std::exception& error) {
    std::cerr << "RootOutput destructor: " << error.what() << '\n';
  }
}

bool RootOutput::IsOpen() const {
  return fFile && fFile->IsOpen() && !fFile->IsZombie();
}

void RootOutput::RequireOpen() const {
  if (!IsOpen()) throw std::logic_error("RootOutput: no open file.");
  if (fFile->TestBit(TFile::kWriteError))
    throw std::runtime_error("RootOutput: file has a write error.");
}

void RootOutput::OpenFile(const std::string& fileName) {
  if (fFile) throw std::logic_error("RootOutput: close the current file before opening another.");
  if (fileName.empty()) throw std::invalid_argument("RootOutput: empty file name.");
  TDirectory::TContext context;
  std::unique_ptr<TFile> file(TFile::Open(fileName.c_str(), "CREATE"));
  if (!file || file->IsZombie() || !file->IsOpen() || !file->IsWritable())
    throw std::runtime_error("RootOutput: cannot create file: " + fileName);
  TNamed schema("SchemaVersion", "1");
  TNamed unit("EnergyUnit", "keV");
  if (file->WriteTObject(&schema) <= 0 || file->WriteTObject(&unit) <= 0 ||
      file->TestBit(TFile::kWriteError))
    throw std::runtime_error("RootOutput: cannot write file metadata: " + fileName);
  fFile = std::move(file);
}

TTree* RootOutput::CreateTree(const char* name, const char* title, bool incident) {
  TDirectory::TContext context(fFile.get());
  auto* tree = new TTree(name, title, 99, fFile.get());
  // Positive entry limits avoid adaptive byte-based buffer sizing for mostly zero data.
  tree->SetAutoFlush(10000);
  tree->SetAutoSave(100000);
  const bool valid = tree->Branch("RunID", &fRunId, "RunID/I") &&
                     tree->Branch("EventID", &fEventId, "EventID/I") &&
                     tree->Branch("Edep", &fEdep, "Edep/D") &&
                     tree->Branch("DetectorName", &fDetectorName) &&
                     (!incident || tree->Branch("TrackID", &fTrackId, "TrackID/I"));
  if (!valid) {
    delete tree;
    throw std::runtime_error("RootOutput: cannot create tree branches.");
  }
  return tree;
}

void RootOutput::Fill(TTree& tree) {
  TDirectory::TContext context(fFile.get());
  if (tree.Fill() < 0 || fFile->TestBit(TFile::kWriteError))
    throw std::runtime_error("RootOutput: cannot append row.");
}

void RootOutput::WriteEventRecords(G4int runId, const std::vector<EventEnergyRecord>& records) {
  RequireOpen();
  if (runId < 0) throw std::invalid_argument("RootOutput: negative RunID.");
  for (const auto& record : records) Validate(runId, record.eventId, record.edep, record.detectorName);
  if (!fEventTree) fEventTree = CreateTree("EventEnergy", "Energy per event and detector; Edep in keV", false);
  for (const auto& record : records) {
    fRunId = runId;
    fEventId = record.eventId;
    fEdep = record.edep / keV;
    fDetectorName = record.detectorName;
    Fill(*fEventTree);
  }
}

void RootOutput::WriteIncidentRecords(G4int runId, const std::vector<IncidentEnergyRecord>& records) {
  RequireOpen();
  if (runId < 0) throw std::invalid_argument("RootOutput: negative RunID.");
  for (const auto& record : records) {
    Validate(runId, record.eventId, record.edep, record.detectorName);
    if (record.trackId <= 0) throw std::invalid_argument("RootOutput: non-positive TrackID.");
  }
  if (!fIncidentTree)
    fIncidentTree = CreateTree("IncidentEnergy", "Energy per incident root and detector; Edep in keV", true);
  for (const auto& record : records) {
    fRunId = runId;
    fEventId = record.eventId;
    fTrackId = record.trackId;
    fEdep = record.edep / keV;
    fDetectorName = record.detectorName;
    Fill(*fIncidentTree);
  }
}

void RootOutput::CloseFile() {
  if (!fFile) return;
  TDirectory::TContext context;
  // Move ownership locally so failure still closes the file and resets the writer.
  const bool hasTrees = fEventTree || fIncidentTree;
  auto file = std::move(fFile);
  fEventTree = nullptr;
  fIncidentTree = nullptr;
  const auto bytes = file->Write("", TObject::kOverwrite);
  const bool writeFailed = (hasTrees && bytes <= 0) || file->TestBit(TFile::kWriteError);
  file->Close();
  if (writeFailed || file->TestBit(TFile::kWriteError))
    throw std::runtime_error("RootOutput: failed to finalize file: " + std::string(file->GetName()));
}

}  // namespace cupid
