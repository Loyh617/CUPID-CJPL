/// \file include/record/RecordSchema.hh
/// \brief Describe ordered record fields independently of an output backend.
///
/// Specialize RecordTraits beside each record struct. Describe binds its detector
/// key and Column member pointers; types are inferred, names and units explicit.
/// RunID is reserved for run context. Only scalar int/float/double/string fields
/// are supported. Descriptions must be identical on master and worker threads.
/// Example: Column("Edep", &Record::edep, keV, "keV") stores edep / keV.
/// A descriptor returned by RecordTraits::Get() must have static const lifetime;
/// extraction and validation must be deterministic and must not modify records.

#pragma once

#include "globals.hh"

#include <cmath>
#include <functional>
#include <regex>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace cupid {
enum class ColumnType { Int, Float, Double, String };
using ColumnValue = std::variant<G4int, G4float, G4double, G4String>;

inline bool IsRecordIdentifier(const G4String& name) {
  static const std::regex pattern("[A-Za-z_][A-Za-z0-9_]*");
  return std::regex_match(name, pattern);
}

struct ColumnInfo {
  G4String name;
  ColumnType type;
  G4double scale = 1.;  // Divide the internal value by this output-unit factor.
  G4String unit;
  bool operator==(const ColumnInfo& other) const {
    return name == other.name && type == other.type && scale == other.scale && unit == other.unit;
  }
};

struct RecordSchema {
  G4String name;
  std::vector<ColumnInfo> columns;
  bool operator==(const RecordSchema& other) const {
    return name == other.name && columns == other.columns;
  }
  void Validate() const {
    if (!IsRecordIdentifier(name) || columns.empty())
      throw std::invalid_argument("RecordSchema: invalid dataset name or empty columns.");
    std::set<G4String> names{"RunID"};
    for (const auto& column : columns) {
      if (!IsRecordIdentifier(column.name) || !names.insert(column.name).second)
        throw std::invalid_argument("RecordSchema: invalid, duplicate or reserved column name.");
      if (!std::isfinite(column.scale) || column.scale <= 0. ||
          (!column.unit.empty() && !IsRecordIdentifier(column.unit)))
        throw std::invalid_argument("RecordSchema: invalid unit or conversion factor.");
      switch (column.type) {
        case ColumnType::Float: case ColumnType::Double: break;
        case ColumnType::Int: case ColumnType::String:
          if (column.scale != 1. || !column.unit.empty())
            throw std::invalid_argument("RecordSchema: units require floating-point fields.");
          break;
        default: throw std::invalid_argument("RecordSchema: unsupported column type.");
      }
    }
  }
  G4String Title() const {
    G4String title = name;
    for (const auto& column : columns)
      if (!column.unit.empty()) title += "; " + column.name + " in " + column.unit;
    return title + "; CUPID schema 4";
  }
};

template<class Record> struct RecordField {
  ColumnInfo info;
  std::function<ColumnValue(const Record&)> read;
};

template<class Record, class T>
RecordField<Record> Column(const G4String& name, T Record::* member,
                           G4double scale = 1., const G4String& unit = "") {
  static_assert(std::is_same_v<T, G4int> || std::is_same_v<T, G4float> ||
                std::is_same_v<T, G4double> || std::is_same_v<T, G4String>,
                "Record columns support only G4int, G4float, G4double and G4String.");
  if (!member) throw std::invalid_argument("RecordSchema: null column member.");
  constexpr auto type = std::is_same_v<T, G4int> ? ColumnType::Int :
                        std::is_same_v<T, G4float> ? ColumnType::Float :
                        std::is_same_v<T, G4double> ? ColumnType::Double : ColumnType::String;
  return {{name, type, scale, unit}, [member, scale](const Record& record) -> ColumnValue {
    if constexpr (std::is_floating_point_v<T>) {
      const auto value = static_cast<T>((record.*member) / scale);
      if (!std::isfinite(value)) throw std::invalid_argument("RecordSchema: non-finite column value.");
      return value;
    } else return record.*member;
  }};
}

template<class Record> struct RecordDescription {
  RecordSchema schema;
  G4String Record::* detector;
  std::vector<RecordField<Record>> fields;
  std::function<void(const Record&)> validate;
  RecordDescription WithValidation(std::function<void(const Record&)> check) && {
    validate = std::move(check);
    return std::move(*this);
  }
};

template<class Record> G4String Record::* DetectorKey(G4String Record::* member) { return member; }

template<class Record, class... Fields>
RecordDescription<Record> Describe(const G4String& name, G4String Record::* detector,
                                    Fields... fields) {
  if (!detector) throw std::invalid_argument("RecordSchema: null detector member.");
  RecordDescription<Record> result{{name, {}}, detector, {std::move(fields)...}, {}};
  for (const auto& field : result.fields) result.schema.columns.push_back(field.info);
  result.schema.Validate();
  return result;
}

template<class Record> struct RecordTraits;
}  // namespace cupid
