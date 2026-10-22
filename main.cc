#include "ActionInitialization.hh"
#include "DetectorConstruction.hh"
// 2026-10-18: retired the transport-only baseline for explicit physics modules.
// Restore the old include and initialization together only to reproduce that baseline.
// #include "FTFP_BERT.hh"
#include "PhysicsList.hh"
#include "G4RunManager.hh"
#include "G4RunManagerFactory.hh"
#include "G4UImanager.hh"
#include "G4Version.hh"
#include "G4ios.hh"
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#if CUPID_WITH_VIS
#include "G4UIExecutive.hh"
#include "G4VisExecutive.hh"
#endif

namespace {
struct Options {
  std::string macro;
  int threads = 2;
  bool serial = false;
  bool visualization = false;
  bool help = false;
};
Options Parse(int argc, char** argv) {
  Options options;
  bool threadsGiven = false;
  for (int i=1; i<argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") options.help = true;
    else if (arg == "--serial") options.serial = true;
    else if (arg == "--vis") options.visualization = true;
    else if (arg == "--threads") {
      if (++i >= argc) throw std::invalid_argument("Missing value after --threads");
      const std::string value = argv[i];
      std::size_t used = 0;
      options.threads = std::stoi(value, &used);
      if (used != value.size() || options.threads < 1 || options.threads > 256)
        throw std::invalid_argument("--threads must be an integer in [1,256]");
      threadsGiven = true;
    } else if (!arg.empty() && arg[0] != '-' && options.macro.empty()) options.macro = arg;
    else throw std::invalid_argument("Unknown or duplicate argument: " + arg);
  }
  if (options.serial && threadsGiven) throw std::invalid_argument("Use --serial or --threads, not both");
  return options;
}
}

int main(int argc, char** argv) {
  try {
    const auto options = Parse(argc, argv);
    if (options.help) {
      std::cout << "Usage: cupid-sim [file.mac] [--threads N | --serial] [--vis]\n"
                   "Default: 2 worker threads; no macro starts interactive visualization.\n"
                   "--vis also enables visualization commands in a batch macro.\n";
      return 0;
    }
    const bool interactive = options.macro.empty();
    const bool visualization = interactive || options.visualization;
#if !CUPID_WITH_VIS
    if (visualization) throw std::runtime_error("Built without visualization; pass a batch .mac file.");
#endif
    const auto requested = interactive ? std::string("vis.mac") : options.macro;
    std::filesystem::path macro = requested;
    if (!std::filesystem::is_regular_file(macro)) macro = std::filesystem::path(CUPID_MACRO_DIR)/requested;
    if (!std::filesystem::is_regular_file(macro)) throw std::runtime_error("Macro file not found: "+requested);
    macro = std::filesystem::absolute(macro);

    std::unique_ptr<G4RunManager> runManager(G4RunManagerFactory::CreateRunManager(
        options.serial ? G4RunManagerType::SerialOnly : G4RunManagerType::MTOnly));
    if (!options.serial) runManager->SetNumberOfThreads(options.threads);
    runManager->SetUserInitialization(new cupid::DetectorConstruction);
    // 2026-10-18: retain the old baseline as comments per repository policy.
    // Restore only when deliberately reproducing the previous physics setup.
    // Transport baseline only: not a validated radioactive calibration physics model.
    // auto* physics = new FTFP_BERT;
    // physics->SetVerboseLevel(0);
    // runManager->SetUserInitialization(physics);
    runManager->SetUserInitialization(new cupid::PhysicsList(0));
    auto* actions = new cupid::ActionInitialization;
    runManager->SetUserInitialization(actions);

#if CUPID_WITH_VIS
    std::unique_ptr<G4UIExecutive> session;
    if (interactive) session = std::make_unique<G4UIExecutive>(1, argv);
    std::unique_ptr<G4VisExecutive> vis;
    if (visualization) {
      vis = std::make_unique<G4VisExecutive>("warnings");
      vis->Initialize();
    }
#endif
    auto* ui = G4UImanager::GetUIpointer();
    ui->SetMacroSearchPath(".:" + macro.parent_path().string() + ":" + CUPID_MACRO_DIR);
    ui->ParseMacroSearchPath();
    // 2026-10-18: retired the old physics label; restore only with the old list.
    // G4cout << "CUPID_ENV " << G4Version << " physics=FTFP_BERT mode="
    G4cout << "CUPID_ENV " << G4Version << " physics=CUPIDPhysicsList-v1 mode="
           << (options.serial ? "serial" : "MT") << " threads="
           << (options.serial ? 1 : options.threads) << G4endl;
    ui->ExecuteMacroFile(macro.string().c_str());
    if (actions->HasGeneratorError()) return 1;
    if (ui->GetLastReturnCode() != 0) {
      G4cerr << "CUPID_ERROR: macro failed, Geant4 status=" << ui->GetLastReturnCode() << G4endl;
      return 1;
    }
#if CUPID_WITH_VIS
    if (session) session->SessionStart();
    if (actions->HasGeneratorError()) return 1;
#endif
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "CUPID_ERROR: " << error.what() << '\n';
    return 2;
  }
}

