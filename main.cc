/// \file main.cc
/// \brief Start the simulation, configure user initialization and capture per-run logs.
///
/// Run cupid-sim [file.mac] [--threads N | --serial] [--vis] [--output DIR].
/// main() creates RunLog, the run manager, DetectorConstruction, PhysicsList
/// and ActionInitialization, then executes the macro or interactive session.
/// RunAction creates one directory per run for its ROOT files and run.log.

#include "ActionInitialization.hh"
#include "DetectorConstruction.hh"
#include "output/RunLog.hh"
#include "PhysicsList.hh"

#include "G4RunManager.hh"
#include "G4RunManagerFactory.hh"
#include "G4UImanager.hh"
#include "G4Version.hh"
#include "G4ios.hh"
#if CUPID_WITH_VIS
#include "G4UIExecutive.hh"
#include "G4VisExecutive.hh"
#endif

#include "RVersion.h"

#include <iomanip>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>


namespace {
struct Options {
  std::string macro;
  std::string outputDirectory;
  int threads = 2;  // Default worker count; override with --threads N or select --serial.
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
    else if (arg == "--output") {
      if (++i >= argc || !options.outputDirectory.empty())
        throw std::invalid_argument("Missing or duplicate --output directory");
      options.outputDirectory = argv[i];
      if (options.outputDirectory.empty()) throw std::invalid_argument("Empty --output directory");
    }
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
  std::unique_ptr<cupid::RunLog> log;
  try {
    const auto options = Parse(argc, argv);
    if (options.help) {
      std::cout << "Usage: cupid-sim [file.mac] [--threads N | --serial] [--vis] [--output DIR]\n"
                   "Default: 2 worker threads; no macro starts interactive visualization.\n"
                   "--vis also enables visualization commands in a batch macro.\n"
                   "--output selects the parent directory; each run creates its own subdirectory.\n"
                   "Default parent output directory: ./results/runs\n"
                   "After initialization, /cupid/output/directory DIR overrides the parent for subsequent runs.\n"
                   "After /run/initialize, /cupid/output/fileName PREFIX sets the ROOT filename prefix.\n";
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

    // RunAction creates directories lazily at run start; the macro can override this parent.
    const auto outputDirectory = options.outputDirectory.empty()
        ? std::string("./results/runs") : options.outputDirectory;
    log = std::make_unique<cupid::RunLog>();
    std::cout << "CUPID_OUTPUT_DIRECTORY " << outputDirectory << '\n'
              << "CUPID_VERSIONS " << G4Version << " ROOT=" << ROOT_RELEASE << '\n'
              << "CUPID_COMMAND";
    for (int i = 0; i < argc; ++i) std::cout << " " << std::quoted(argv[i]);
    std::cout << "\nCUPID_CWD " << std::filesystem::current_path().string()
              << "\nCUPID_RECORD_POLICY EventEnergy: positive detector totals only; IncidentEnergy: positive detector-local root totals only."
              << "\nCUPID_RESULT_NOTE Energies are unweighted keV, without detector response or a time window."
              << " Use processed_events, not tree row counts, for the simulated event count.\n";
    log->RecordMacro(macro.string(), {".", macro.parent_path().string(), CUPID_MACRO_DIR});

    std::unique_ptr<G4RunManager> runManager(G4RunManagerFactory::CreateRunManager(
        options.serial ? G4RunManagerType::SerialOnly : G4RunManagerType::MTOnly));
    if (!options.serial) runManager->SetNumberOfThreads(options.threads);
    auto* detector = new cupid::DetectorConstruction;
    runManager->SetUserInitialization(detector);
    // Constructor argument controls physics verbosity; model choices and cuts are in PhysicsList.cc.
    runManager->SetUserInitialization(new cupid::PhysicsList(0));
    auto* actions = new cupid::ActionInitialization(outputDirectory, detector->GetGeometryManager(), log.get());
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
    log->Check();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "CUPID_ERROR: " << error.what() << '\n';
    return 2;
  }
}

