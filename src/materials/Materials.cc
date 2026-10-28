/// \file src/materials/Materials.cc
/// \brief Implementation: Build the shared element and material library and resolve exact names.
///
/// DetectorConstruction calls Build() during geometry initialization before
/// worker transport. Builders use Get(materialName); repeated Build() calls
/// reuse existing entries. Geant4 owns the resulting elements and materials.

#include "materials/Materials.hh"
#include "G4Material.hh"
#include "G4NistManager.hh"
#include "G4PhysicalConstants.hh"
#include "G4SystemOfUnits.hh"
#include <stdexcept>

namespace cupid {
void Materials::Build() {
  auto* nist = G4NistManager::Instance();

  // 1. Elements: natural isotopic compositions from Geant4 11.4.1.
  // Define enriched elements separately when their abundances are available.
  auto* H  = nist->FindOrBuildElement("H");
  auto* C  = nist->FindOrBuildElement("C");
  auto* N  = nist->FindOrBuildElement("N");
  auto* O  = nist->FindOrBuildElement("O");
  auto* F  = nist->FindOrBuildElement("F");
  auto* Cu = nist->FindOrBuildElement("Cu");
  auto* Pb = nist->FindOrBuildElement("Pb");
  auto* Ge = nist->FindOrBuildElement("Ge");
  auto* Li = nist->FindOrBuildElement("Li");
  auto* Mo = nist->FindOrBuildElement("Mo");
  auto* Fe = nist->FindOrBuildElement("Fe");

  // 2. Project materials, one recipe per block.
  // Recipe reference: Loyh617/SimulationTool, MaterialLibrary.cc
  // blob 6312b7f245280b2972246dedc498c2ddbb6dbfe1.
  // Guards allow repeated Build() calls without duplicate material entries.

  // Vacuum: hydrogen gas at the universe mean density.
  if (!G4Material::GetMaterial("Vacuum", false)) {
    auto* vacuum = new G4Material("Vacuum", CLHEP::universe_mean_density, 1,
                                 kStateGas, 2.73*kelvin, 3e-18*pascal);
    vacuum->AddElementByNumberOfAtoms(H, 1);
  }

  // Air: simplified reference recipe, mass fractions (not volume fractions).
  if (!G4Material::GetMaterial("Air", false)) {
    auto* air = new G4Material("Air", 1.29e-3*g/cm3, 2, kStateGas);
    air->AddElementByMassFraction(N, 0.70);
    air->AddElementByMassFraction(O, 0.30);
  }

  // Copper.
  if (!G4Material::GetMaterial("Copper", false)) {
    auto* copper = new G4Material("Copper", 8.96*g/cm3, 1, kStateSolid);
    copper->AddElementByNumberOfAtoms(Cu, 1);
  }

  // Lead.
  if (!G4Material::GetMaterial("Lead", false)) {
    auto* lead = new G4Material("Lead", 11.35*g/cm3, 1, kStateSolid);
    lead->AddElementByNumberOfAtoms(Pb, 1);
  }

  // Steel: reference Fe/C approximation; NOT stainless steel.
  if (!G4Material::GetMaterial("Steel", false)) {
    auto* steel = new G4Material("Steel", 7.874*g/cm3, 2, kStateSolid);
    steel->AddElementByMassFraction(Fe, 0.96);
    steel->AddElementByMassFraction(C, 0.04);
  }

  // PTFE: C2F4 stoichiometry.
  if (!G4Material::GetMaterial("PTFE", false)) {
    auto* ptfe = new G4Material("PTFE", 2.2*g/cm3, 2, kStateSolid);
    ptfe->AddElementByNumberOfAtoms(C, 2);
    ptfe->AddElementByNumberOfAtoms(F, 4);
  }

  // Natural Li2MoO4: retain the reference temperature, 10 mK.
  // Setting temperature does not itself compute thermal contraction or response.
  if (!G4Material::GetMaterial("Li2MoO4", false)) {
    auto* lmo = new G4Material("Li2MoO4", 3.07*g/cm3, 3, kStateSolid, 0.01*kelvin);
    lmo->AddElementByNumberOfAtoms(Li, 2);
    lmo->AddElementByNumberOfAtoms(Mo, 1);
    lmo->AddElementByNumberOfAtoms(O, 4);
  }

  // NTD Ge: pure natural germanium approximation; no dopant model yet.
  if (!G4Material::GetMaterial("NTD_Ge", false)) {
    auto* ntd = new G4Material("NTD_Ge", 5.323*g/cm3, 1, kStateSolid);
    ntd->AddElementByNumberOfAtoms(Ge, 1);
  }

  // 3. Standard materials used by the existing geometry.
  // Keep their Geant4 definitions; especially do not replace stainless steel
  // with the Fe/C recipe above. NIST manages their constituent elements.
  nist->FindOrBuildMaterial("G4_Galactic");
  nist->FindOrBuildMaterial("G4_Cu");
  nist->FindOrBuildMaterial("G4_Al");
  nist->FindOrBuildMaterial("G4_STAINLESS-STEEL");
}

G4Material* Materials::Get(const G4String& name) const {
  auto* material = G4Material::GetMaterial(name, false);
  if (!material) throw std::runtime_error("Undefined material: " + name);
  return material;
}
}

