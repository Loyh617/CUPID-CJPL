/// \file include/record/RecordBatch.hh
/// \brief Own typed record vectors and expose synchronous, non-owning batch views.
///
/// RecordManager creates a RecordBatch for each selected record type. View() keeps
/// the original vector in place; callers must keep its owner alive and unchanged
/// until Write() returns. Schemas come from static RecordTraits descriptions and
/// are available even for empty batches. No list of concrete record types is needed.

#pragma once

#include "record/RecordSchema.hh"

#include <any>
#include <cstddef>
#include <vector>

namespace cupid {
class RecordBatchView {
 public:
  template<class Record> explicit RecordBatchView(const std::vector<Record>& rows)
      : fRows(&rows), fSchema(&RecordTraits<Record>::Get().schema), fSize(rows.size()),
        fDetector([](const void* data, std::size_t row) -> const G4String& {
          return static_cast<const std::vector<Record>*>(data)->at(row).*RecordTraits<Record>::Get().detector;
        }),
        fValue([](const void* data, std::size_t row, std::size_t column) {
          return RecordTraits<Record>::Get().fields.at(column).read(
              static_cast<const std::vector<Record>*>(data)->at(row));
        }),
        fValidate([](const void* data, std::size_t row) {
          const auto& check = RecordTraits<Record>::Get().validate;
          if (check) check(static_cast<const std::vector<Record>*>(data)->at(row));
        }) {}
  template<class Record> RecordBatchView(std::vector<Record>&&) = delete;
  template<class Record> RecordBatchView(const std::vector<Record>&&) = delete;
  const RecordSchema& Schema() const { return *fSchema; }
  std::size_t Size() const { return fSize; }
  const G4String& DetectorName(std::size_t row) const { return fDetector(fRows, row); }
  ColumnValue Value(std::size_t row, std::size_t column) const { return fValue(fRows, row, column); }
  void ValidateRow(std::size_t row) const { fValidate(fRows, row); }

 private:
  const void* fRows;
  const RecordSchema* fSchema;
  std::size_t fSize;
  const G4String& (*fDetector)(const void*, std::size_t);
  ColumnValue (*fValue)(const void*, std::size_t, std::size_t);
  void (*fValidate)(const void*, std::size_t);
};

class RecordBatch {
 public:
  template<class Record> static RecordBatch Create() {
    return {std::vector<Record>{},
        [](const std::any& data) { return RecordBatchView(std::any_cast<const std::vector<Record>&>(data)); },
        [](std::any& data) { std::any_cast<std::vector<Record>&>(data).clear(); }};
  }
  RecordBatch(const RecordBatch&) = delete;
  RecordBatch& operator=(const RecordBatch&) = delete;
  RecordBatch(RecordBatch&&) noexcept = default;
  RecordBatch& operator=(RecordBatch&&) noexcept = default;
  template<class Record> std::vector<Record>* Get() { return std::any_cast<std::vector<Record>>(&fData); }
  template<class Record> const std::vector<Record>* Get() const {
    return std::any_cast<std::vector<Record>>(&fData);
  }
  RecordBatchView View() const { return fView(fData); }
  void Clear() { fClear(fData); }

 private:
  RecordBatch(std::any data, RecordBatchView (*view)(const std::any&), void (*clear)(std::any&))
      : fData(std::move(data)), fView(view), fClear(clear) {}
  std::any fData;
  RecordBatchView (*fView)(const std::any&);
  void (*fClear)(std::any&);
};
}  // namespace cupid
