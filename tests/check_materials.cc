#include "materials/Materials.hh"
#include "G4Element.hh"
#include "G4Material.hh"
#include "G4PhysicalConstants.hh"
#include "G4SystemOfUnits.hh"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
bool Near(double actual, double expected) {
  return std::abs(actual - expected) <= 1e-10 * std::abs(expected);
}
double Fraction(const G4Material* material, int z) {
  for (std::size_t i = 0; i < material->GetNumberOfElements(); ++i) {
    if (static_cast<int>(material->GetElement(i)->GetZ()) == z)
      return material->GetFractionVector()[i];
  }
  throw std::runtime_error("Missing constituent element");
}
double AtomRatio(const G4Material* material, int z1, int z2) {
  double n1 = 0., n2 = 0.;
  for (std::size_t i = 0; i < material->GetNumberOfElements(); ++i) {
    const int z = static_cast<int>(material->GetElement(i)->GetZ());
    if (z == z1) n1 = material->GetVecNbOfAtomsPerVolume()[i];
    if (z == z2) n2 = material->GetVecNbOfAtomsPerVolume()[i];
  }
  Require(n2 > 0., "Missing denominator element");
  return n1 / n2;
}
}

int main() {
  try {
    cupid::Materials materials;
    materials.Build();
    const auto materialCount = G4Material::GetMaterialTable()->size();
    const auto elementCount = G4Element::GetElementTable()->size();
    cupid::Materials secondLibrary;
    secondLibrary.Build();
    Require(G4Material::GetMaterialTable()->size() == materialCount, "Duplicate materials");
    Require(G4Element::GetElementTable()->size() == elementCount, "Duplicate elements");

    struct Recipe { const char* name; double density; };
  // 2026-10-18: disabled to require exact material names.
  // Restore only if alias support is explicitly requested again.
  //     const Recipe recipes[] = {
  //         {"Vacuum", CLHEP::universe_mean_density}, {"WorldAir", 1.29e-3*g/cm3},
  //         {"Copper", 8.96*g/cm3}, {"Lead", 11.35*g/cm3}, {"Steel", 7.874*g/cm3},
  //         {"PTFE", 2.2*g/cm3}, {"LMO_Crystal", 3.07*g/cm3}, {"NTD_Ge", 5.323*g/cm3}};
    const Recipe recipes[] = {
        {"Vacuum", CLHEP::universe_mean_density}, {"Air", 1.29e-3*g/cm3},
        {"Copper", 8.96*g/cm3}, {"Lead", 11.35*g/cm3}, {"Steel", 7.874*g/cm3},
        {"PTFE", 2.2*g/cm3}, {"Li2MoO4", 3.07*g/cm3}, {"NTD_Ge", 5.323*g/cm3}};
    for (const auto& recipe : recipes) {
      auto* material = materials.Get(recipe.name);
      Require(Near(material->GetDensity(), recipe.density), "Incorrect density");
      double sum = 0.;
      for (std::size_t i = 0; i < material->GetNumberOfElements(); ++i) {
        sum += material->GetFractionVector()[i];
        Require(material->GetElement(i)->GetNumberOfIsotopes() > 0, "Missing isotopes");
      }
      Require(Near(sum, 1.), "Mass fractions do not sum to one");
      std::cout << "PASS material " << recipe.name << '\n';
    }
    Require(Near(Fraction(materials.Get("Air"), 7), 0.70), "Air nitrogen mass fraction");
    Require(Near(Fraction(materials.Get("Air"), 8), 0.30), "Air oxygen mass fraction");
    Require(Near(Fraction(materials.Get("Steel"), 26), 0.96), "Steel iron mass fraction");
    Require(Near(Fraction(materials.Get("Steel"), 6), 0.04), "Steel carbon mass fraction");
    Require(Near(AtomRatio(materials.Get("PTFE"), 9, 6), 2.), "PTFE F:C ratio");
  // 2026-10-18: disabled to require exact material names.
  // Restore only if alias support is explicitly requested again.
  //     auto* lmo = materials.Get("LMO_Crystal");
    auto* lmo = materials.Get("Li2MoO4");
    Require(Near(AtomRatio(lmo, 3, 42), 2.), "LMO Li:Mo ratio");
    Require(Near(AtomRatio(lmo, 8, 42), 4.), "LMO O:Mo ratio");
    Require(Near(lmo->GetTemperature(), 0.01*kelvin), "LMO temperature");
    Require(lmo->GetState() == kStateSolid, "LMO state");
  // 2026-10-18: disabled to require exact material names.
  // Restore only if alias support is explicitly requested again.
  //     Require(materials.Get("CUPID.LMO.Natural") == lmo && materials.Get("Li2MoO4") == lmo,
  //             "Crystal aliases must refer to the same material");
  //     Require(materials.Get("WorldAir") == materials.Get("Air"), "Air alias");
    for (const auto* oldName : {"WorldAir", "LMO_Crystal", "CUPID.LMO.Natural"}) {
      bool rejectedAlias = false;
      try { materials.Get(oldName); }
      catch (const std::runtime_error&) { rejectedAlias = true; }
      Require(rejectedAlias, "Material alias was accepted");
    }
    Require(Near(materials.Get("Vacuum")->GetTemperature(), 2.73*kelvin), "Vacuum temperature");
    Require(Near(materials.Get("Vacuum")->GetPressure(), 3e-18*pascal), "Vacuum pressure");
    for (const auto* name : {"G4_Galactic", "G4_Cu", "G4_Al", "G4_STAINLESS-STEEL"})
      Require(materials.Get(name) == G4Material::GetMaterial(name), "Standard material lookup");
    Require(materials.Get("Steel") != materials.Get("G4_STAINLESS-STEEL"), "Steel was substituted");
    bool rejected = false;
    try { materials.Get("NotAMaterial"); }
    catch (const std::runtime_error&) { rejected = true; }
    Require(rejected, "Unknown material accepted");
  // 2026-10-18: disabled to require exact material names.
  // Restore only if alias support is explicitly requested again.
  //     std::cout << "PASS material composition, aliases and repeated initialization\n";
    std::cout << "PASS material composition, exact names and repeated initialization\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
