/// \file include/output/VOutput.hh
/// \brief Define schema booking and synchronous batch writing for output backends.
///
/// Actions pass recorder schemas before opening files and batch views after each
/// event. Backends never retain a view. Schemas and detector catalogs must agree
/// on all threads. Typed convenience functions use the record's own description.

#pragma once

#include "record/RecordBatch.hh"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace cupid {
struct OutputSummary {
  std::map<G4String, std::int64_t> rows;
  std::int64_t Count(const G4String& name) const {
    const auto found = rows.find(name);
    return found == rows.end() ? 0 : found->second;
  }
};

class VOutput {
 public:
  virtual ~VOutput() = default;
  virtual std::string GetFileExtension() const = 0;
  virtual void Book(const RecordSchema& schema) = 0;
  template<class Record> void Book(const std::vector<Record>&) { Book(RecordTraits<Record>::Get().schema); }
  virtual void SetDetectorNames(const std::vector<G4String>& names) = 0;
  virtual std::vector<std::string> GetFileNames(const std::string& baseName) const = 0;
  virtual void OpenFile(const std::string& fileName) = 0;
  virtual bool IsOpen() const = 0;
  virtual void Write(G4int runId, const RecordBatchView& records) = 0;
  template<class Record> void Write(G4int runId, const std::vector<Record>& records) {
    Write(runId, RecordBatchView(records));
  }
  virtual void CloseFile() = 0;
  virtual OutputSummary Inspect(const std::string& fileName) const = 0;
};
}  // namespace cupid
