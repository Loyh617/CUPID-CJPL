// Shared ROOT I/O for the three offline LMO incident-energy macros.
#pragma once

#include "TFile.h"
#include "TKey.h"
#include "TLeaf.h"
#include "TNamed.h"
#include "TTree.h"
#include "TUUID.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>

namespace cupid_analysis {
namespace fs = std::filesystem;

inline void Require(bool ok, const std::string& message) {
  if (!ok) throw std::runtime_error(message);
}

inline TLeaf& Column(TTree& tree, const char* name, const char* type) {
  auto* leaf = tree.GetLeaf(name);
  Require(leaf && std::string(leaf->GetTypeName()) == type &&
          !leaf->GetLeafCount() && leaf->GetLenStatic() == 1,
          std::string(tree.GetName()) + ": missing or incompatible column " + name);
  return *leaf;
}

inline void Read(TTree& tree, Long64_t row) {
  Require(tree.GetEntry(row) > 0, std::string("Cannot read row from ") + tree.GetName());
}

inline void Keep(TTree& tree) {
  Require(tree.Fill() >= 0, std::string("Cannot write row to ") + tree.GetName());
}

// All stages copy the incident schema, including any additional columns.
// Publish the completed sibling file without replacing an existing path.
template<class Transform>
void Process(const fs::path& input, const fs::path& requestedOutput,
             const char* suffix, const std::string& previous, const std::string& stage,
             const std::string& settings, Transform transform) {
  const fs::path output = requestedOutput.empty()
      ? input.parent_path() / (input.stem().string() + suffix + ".root") : requestedOutput;
  Require(!fs::exists(output), "Output already exists: " + output.string());
  TFile source(input.c_str(), "READ");
  Require(!source.IsZombie(), "Cannot open " + input.string());
  auto* marker = source.Get<TNamed>("AnalysisStage");
  Require((marker ? marker->GetTitle() : std::string{}) == previous,
          "Unexpected input stage for " + stage);

  std::set<std::string> detectors;
  const std::regex lmoName("LMO[0-9]+");
  TIter next(source.GetListOfKeys());
  while (auto* key = static_cast<TKey*>(next())) {
    if (std::regex_match(key->GetName(), lmoName))
      detectors.insert(key->GetName());
  }
  Require(!detectors.empty(), "No LMO detector trees in " + input.string());

  const fs::path temporary = output.string() + "." + TUUID().AsString() + ".part";
  TFile dest(temporary.c_str(), "CREATE");
  Require(!dest.IsZombie(), "Cannot create output beside " + output.string());
  try {
    for (const auto& name : detectors) {
      auto* tree = source.Get<TTree>(name.c_str());
      Require(tree, "Missing detector tree " + name);
      for (auto field : {"RunID", "EventID", "TrackID", "PDG"}) Column(*tree, field, "Int_t");
      Column(*tree, "Edep", "Double_t");
      dest.cd();
      std::unique_ptr<TTree> result(tree->CloneTree(0));
      Require(bool(result), "Cannot copy tree structure for " + name);
      transform(*tree, *result);
      Require(result->Write("", TObject::kOverwrite) > 0, "Cannot write " + name);
      std::cout << name << ": " << tree->GetEntries() << " -> " << result->GetEntries() << '\n';
    }
    dest.cd();
    auto* history = source.Get<TNamed>("AnalysisHistory");
    std::string info = history ? std::string(history->GetTitle()) + "\n" : "";
    info += stage + " input=" + input.string() + " " + settings;
    Require(TNamed("AnalysisStage", stage.c_str()).Write() > 0 &&
            TNamed("AnalysisHistory", info.c_str()).Write() > 0, "Cannot write analysis settings");
    dest.Close();
    Require(!dest.TestBit(TFile::kWriteError), "ROOT output write failed");
    fs::create_hard_link(temporary, output);
  } catch (...) {
    dest.Close();
    fs::remove(temporary);
    throw;
  }
  fs::remove(temporary);
  std::cout << "Output: " << output << '\n';
}
}  // namespace cupid_analysis
