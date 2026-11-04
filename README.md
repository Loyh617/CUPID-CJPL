# CUPID-CJPL Simulation Framework

## 1. Overview and Scope

A Geant4 framework for collaborative development of CUPID-CJPL simulations, covering radioactive backgrounds, calibration sources, and signals within LMO crystals. The current simulation endpoint is **energy deposition in LMO**.

Implemented features include the cryostat, inner shielding, LMO array, particle gun, position-sampling utilities, event-level and incident-particle energy recording, and multithreaded ROOT output. Dedicated environmental, copper-frame, cosmic-ray, Th-232/Co-56 calibration, and double-beta generators remain to be implemented. The existing Th-232 macro emits a nuclide through the particle gun.

Current checks cover gamma transport, radioactive-decay execution, and software behavior; no general claim of physics accuracy or validated energy range has been established. Laboratory rock, NTD/LD detectors, optical transport, dose scoring, detector response, electronics, and waveforms are not implemented. **A successful run does not establish physics validity.**

## 2. Quick Start

Install the dependencies in Section 3 first; authenticate with GitHub if the repository is private. Run these commands in a WSL Ubuntu terminal. After cloning, all commands are executed from the repository root:

```bash
git clone https://github.com/Loyh617/CUPID-CJPL.git
cd CUPID-CJPL

# Adjust these paths to your installations.
export CUPID_GEANT4_PREFIX="$HOME/geant4/install"
export CUPID_ROOT_PREFIX="$HOME/root"
source "$CUPID_ROOT_PREFIX/bin/thisroot.sh"
source "$CUPID_GEANT4_PREFIX/bin/geant4.sh"

cmake -S . -B build \
  -DCMAKE_PREFIX_PATH="$CUPID_GEANT4_PREFIX;$CUPID_ROOT_PREFIX" \
  -DCMAKE_BUILD_TYPE=Release -DCUPID_WITH_VIS=OFF
cmake --build build -j4

./build/cupid-sim macros/lmo_only.mac --threads 2 --output results/quickstart
echo "Exit status: $?"
```

This example builds only two LMO crystals and emits **1,000 gamma rays at 1 MeV**. It took approximately **5.3 seconds** on the development machine on 2026-10-30, excluding compilation; hardware and initialization affect runtime. Expect exit status 0, `result=PASS` for the geometry check and `CUPID_RUN_RESULT ... processed_events=1000` in `results/quickstart/<run-prefix>/run.log`, and `run<datetime>_EventEnergy.root` / `run<datetime>_IncidentEnergy.root` files. Each run has its own subdirectory; repeating the command preserves existing runs and selects a free name.

Compilation, execution, and ROOT readback have been checked in the existing development environment. **Independent installation and execution by another member in a clean environment remain pending.** Dependency requirements and installation guidance follow below.

## 3. Dependencies and Verified Environment

| Component | Checked environment / requirement |
| --- | --- |
| Operating system | Windows 11 with WSL Ubuntu 24.04.1 LTS; native Windows builds are unverified |
| Compiler / standard | GCC 13.3.0 / C++17 |
| CMake | 3.28.3 locally; minimum 3.16 |
| Geant4 | Exactly 11.4.1, built with `GEANT4_BUILD_MULTITHREADED=ON`; serial mode also links this installation |
| ROOT | Exactly 6.36.10; Core/RIO/Tree required, with PyROOT for output integration tests |
| Python | Python 3 for automated tests, which are enabled by default |
| Visualization | Optional; Qt 5.15.13 locally, WSLg/X11 for graphics, and corresponding Geant4 UI/visualization support |
| GDML | Not required by the C++ geometry; dependencies of the selected Geant4 installation must still be satisfied |

Use `GEANT4_INSTALL_DATA=ON` when installing Geant4. For Qt visualization, build Geant4 with `GEANT4_USE_QT=ON` and configure this application with `CUPID_WITH_VIS=ON`. The environment scripts set Geant4 data paths and ROOT library paths. See the [Geant4 11.4 Installation Guide](https://geant4.web.cern.ch/documentation/dev/ig_html/InstallationGuide/installguide.html).

The following datasets are installed locally. This is an installation inventory, not a list of datasets used by every run. Follow the Geant4 11.4.1 dataset manifest and do not mix versions.

| Dataset | Version |
| --- | --- |
| G4EMLOW | 8.8 |
| PhotonEvaporation / RadioactiveDecay | 6.1.2 / 6.1.2 |
| G4ENSDFSTATE | 3.0 |
| G4NDL | 4.7.1 |
| G4PARTICLEXS | 4.2 |
| G4PII / G4SAIDDATA | 1.3 / 2.0 |
| G4ABLA / G4INCL | 3.3 / 1.3 |
| RealSurface / G4CHANNELING | 2.2 / 2.0 |

The local ROOT CMake configuration requires Vdt development files. On Ubuntu, install `libvdt-dev`, or add `-DVDT_INCLUDE_DIR=/path/to/include -DVDT_LIBRARY=/path/to/libvdt.so` when configuring with a separate installation. The existing local build cache points to `build/local-deps/vdt/usr/`, which is not tracked by Git and is unavailable in a fresh clone.

The project does not provide a script that automatically installs dependencies or datasets.

## 4. Repository Layout and Extension Points

```text
CUPID-CJPL/
├── main.cc / CMakeLists.txt       # Application entry point and build
├── include/                      # Headers
│   ├── geometry/                 # Geometry components and manager
│   ├── materials/                # Materials
│   ├── generator/                # Generators and sampling toolkit
│   ├── record/                   # Recorders and manager
│   ├── output/                   # Output modules and manager
│   └── *.hh                      # Construction, PhysicsList, Actions, SD/Hit
├── src/                          # Implementations mirroring include/
├── macros/                       # Geometry, run, and visualization macros
├── tests/                        # Regression tests and ROOT checks
├── docs/                         # Existing review and validation material
├── README.md                     # Usage and collaboration guide
└── history.log                   # Change history
```

`DetectorConstruction → geometry/materials`; `PhysicsList → physics processes`; `PrimaryGeneratorAction → generator`; `LMOSensitiveDetector/DetectorHit → raw hits`; `EventAction → record → output`. `RunAction` controls output lifetime, `TrackingAction` supplies track ancestry, and `SteppingAction/StackingAction` provide extension points.

To add a detector, extend `geometry/`, `GeometryManager`, and sensitive-detector attachment. Implement `VGenerator` and register it with `GeneratorManager` to add a source. To add a recorder, define its record struct and statistics class, then add its selection, result vector and Build call to `RecordManager`. Add explicit booking and typed Write functions to the output interfaces/backend, connect them in the Actions, and add implementation files to CMake as needed. Select recorders in `EventAction::DefaultRecorders()`. File comments describe the interfaces. There is currently no separate production analysis package.

## 5. Physics Models and Geometry Configuration

- The custom modular `PhysicsList` combines Livermore electromagnetic physics, additional electromagnetic nuclear interactions, particle and radioactive decay, HP hadron elastic scattering, Shielding hadron inelastic physics, stopping processes, and ion interactions. Constructors are listed below.
- Fission-fragment production, fluorescence, Auger emission, and atomic de-excitation ignoring production cuts are enabled. The radioactive-decay time threshold is `1e60 year`; it is neither a detector integration window nor a modification of nuclear lifetimes.
- The default production range cut is **0.1 mm**, with **0 mm** for protons. Study-specific convergence checks are still required. These are range settings for secondary production, not tracking energy thresholds or maximum step lengths. See [Geant4's explanation](https://geant4.web.cern.ch/documentation/dev/bfad_html/ForApplicationDevelopers/TrackingAndPhysics/thresholdVScut.html).
- Coordinates are right-handed. The origin is the world center, on the cryostat axis at the MXC plate's bottom face; the cryostat axis runs along Y. The world is a 4 m vacuum cube. LMO crystals are 45 mm cubes of natural-abundance `Li2MoO4`, with density 3.07 g/cm³ and material temperature 0.01 K; this temperature does not implement a cryogenic response model. The array is centered at `(0,-350,0) mm`; two crystals are arranged along X with a 5 mm gap. Cryostat and shielding materials mainly include copper, stainless steel, and aluminum.
- Components are initially disabled and enabled by macros; 1–2 LMO crystals are currently supported. Dimensions are in `include/geometry/GeometryParameters.hh`, materials in `src/materials/Materials.cc`. Specify length, energy, and time units explicitly in macros. Directions are dimensionless vectors; ROOT energies are in keV.

```text
G4EmLivermorePhysics          G4EmExtraPhysics
G4DecayPhysics                G4RadioactiveDecayPhysics
G4HadronElasticPhysicsHP       G4HadronPhysicsShielding
G4StoppingPhysics             G4IonElasticPhysics
G4IonQMDPhysics
```

`G4HadronPhysicsShielding` is a hadronic constructor, not the complete Shielding physics list. In Geant4 11.4.1, `G4EmExtraPhysics` enables gamma-, electro-, and muon-nuclear interactions by default.

A Geant4 event currently includes the primary particles and transport of their descendants. Events are not regrouped into detector time windows, and the energy tables do not contain time columns.

The quick-start source is at `(-25,-350,-100) mm`, emitting along `+Z`. The full-geometry `run.mac` example uses `(0,-350,-100) mm`, also emitting 1 MeV gamma rays along `+Z`.

## 6. Running and Configuring the Application

Supported command line:

```text
cupid-sim [file.mac] [--threads N | --serial] [--vis] [--output DIR]
```

The default is 2 worker threads. `--threads` accepts 1–256 and is mutually exclusive with `--serial`. Omitting the macro starts interactive visualization. Batch macros that use visualization commands require `--vis`. Graphics require a visualization-enabled build.

| Macro | Purpose |
| --- | --- |
| `lmo_only.mac` | Minimal example: two LMO crystals and 1,000 gamma events |
| `run.mac` | Self-contained full-component 1,000-event example |
| `check_geometry.mac` | Geometry overlap checks |
| `vis.mac` / `render.mac` | Interactive display / offscreen rendering; the latter requires `--vis` |
| `th232_two_lmo.mac` | Ideal bare Th-232 point source, two LMO crystals, 100,000 events; not a complete calibration-source model |
| `commands.mac` | Comment-only reference for every custom command; not a run preset |

Each of the six runnable macros contains its own geometry, initialization and relevant source or visualization commands. Keep new run macros self-contained rather than splitting configuration into nested macros. The command reference stays in the source directory; the six runnable macros are copied to `build/macros/` during CMake configuration.

Set event counts with `/run/beamOn`, seeds with `/random/setSeeds` (`12345 67890` in the quick-start example), and particle type, energy, position, and direction with `/gun/*`. Enable geometry and set `/cupid/geometry/lmoCount` before `/run/initialize`.

Command-line options set the initial mode, thread count, and output. Macros then execute in order and may update Geant4 parameters where the application state allows; check the actual run log. The main macro is resolved relative to the current directory first, then under `build/macros/`. Submacros are searched in the current directory, main macro directory, and `build/macros/`, in that order. Relative output paths also use the current directory. Prefer explicit `macros/xxx.mac` paths; if using build-directory copies, rerun CMake configuration after editing example macros. See [Geant4 macro execution](https://geant4.web.cern.ch/documentation/dev/bfad_html/ForApplicationDevelopers/GettingStarted/executeProgram.html).

Output directories and ROOT filename prefixes are configured through `output/OutputMessenger`, which uses native `G4GenericMessenger` and is owned by the master/serial `RunAction`. `RunAction` retains the settings and prepares directories/files at run start. Use these commands **after `/run/initialize` and before `/run/beamOn`**, also between runs:

```text
/cupid/output/directory ./results/runs
/cupid/output/fileName th232_calibration
/run/beamOn 100000
/cupid/output/fileName co56_calibration.root
/run/beamOn 100000
/cupid/output/fileName auto
/run/beamOn 1000
```

These commands configure output only; configure the source separately. The default parent is `./results/runs`. `--output DIR` sets the initial parent; `/cupid/output/directory DIR` overrides it for subsequent runs and persists until changed. Relative paths use the process working directory, not the macro directory. Missing parents are created at run start; quote directory paths containing spaces. The first pair is `th232_calibration_EventEnergy.root` and `th232_calibration_IncidentEnergy.root`. An optional `.root` suffix is removed before adding the recorder suffix. `auto` is the default and generates `runYYYYMMDD_HHMMSS_<Recorder>.root`, using the WSL process's local time, to the second. The master determines one name shared by all workers. Custom prefixes must start with an ASCII letter/digit, use only letters/digits/underscore/hyphen/dot, and not end in a dot; paths and spaces are rejected. `CUPID_RUN_DIRECTORY` and `CUPID_OUTPUT_NAME` in `run.log` record the resolved directory and base name even for empty runs.

## 7. Output Format and Minimal Analysis

Each run creates a subdirectory named after its shared ROOT prefix (without `.root` or the recorder suffix). All recorder files and that run's `run.log` are placed inside it; no loose ROOT/log files are written to the parent. The current format remains **schema 4**:

```text
results/runs/
  run20261103_153000/
    run20261103_153000_EventEnergy.root       # LMO0, LMO1 TTrees
    run20261103_153000_IncidentEnergy.root    # LMO0, LMO1 TTrees
    run.log
  run20261103_153000_1/
    run20261103_153000_1_EventEnergy.root
    run20261103_153000_1_IncidentEnergy.root
    run.log
```

| File | Meaning of one row in a detector tree | Columns, in order |
| --- | --- | --- |
| `<prefix>_EventEnergy.root` | Total energy deposited in that detector within one event | `RunID, EventID, Edep` |
| `<prefix>_IncidentEnergy.root` | Total deposited energy attributed to one incident root and its associated descendants in that detector/event | `RunID, EventID, TrackID, PDG, Edep` |

The **tree name identifies the detector**; there is no `DetectorName` branch. Internal recorder data retain `detectorName` for routing. Detector names come from the constructed scoring geometry, must be unique identifiers (`[A-Za-z_][A-Za-z0-9_]*`), and are booked consistently on master and workers before a run. The current geometry registers LMOs; future detector components must also supply their scoring names. A detector without deposits has an empty tree in a retained recorder file. A recorder file with no rows in any detector is removed; a wholly empty run leaves only the log. Unknown detector names in records are errors, not silently created trees.

`RecordManager` stores separate `std::vector<EventEnergyRecord>` and `std::vector<IncidentEnergyRecord>` results. `EventAction` passes the selected vectors by const reference to the matching `OutputManager::Write()` overload. `RunAction` calls `BookEventEnergy()` and/or `BookIncidentEnergy()` before opening files. `RootOutput` lists each table's columns and field assignments directly in separate booking/filling functions, using Geant4's native column APIs and converting `edep / keV` during filling. There is no automatic member discovery or generic record serialization layer.

Table selections and detector catalogs must match on all threads, including workers without events. Each input vector is checked before any of its rows are appended. Run summaries use `rows_EventEnergy` and `rows_IncidentEnergy`. Adding a new record type intentionally requires a corresponding output adapter and Action calls; update the column order, field mappings and readback tests together.

Both recorders are enabled by default. Selecting only one recorder creates only its file. Geant4's `SetNtupleFileName` routes trees into the corresponding file, and native ntuple merging combines workers. Rows are not guaranteed to follow EventID order. A custom prefix remains active until changed. For automatic and custom names, a collision adds `_1`, `_2`, etc. before the recorder suffix (for example, `run20261102_153000_1_EventEnergy.root`). The master reserves the run subdirectory with one filesystem directory-creation operation, so every file from one run shares the same suffix. Existing directories (including empty ones) and same-name files are skipped. Empty runs retain their directory and log. Changing the parent allows the same prefix in a different location. The RunID branch continues to identify the Geant4 run independently of its filename.

`Edep` is unweighted deposited energy in keV, not incident energy. TrackID and PDG identify the detector-local incident root. Re-entry of the same track into the same crystal is grouped together; descendants entering another crystal are attributed separately for that crystal. Only aggregated `Edep > 0` rows are saved. Row counts are not simulated event counts: use `processed_events` from the log as the normalization denominator. Activity, exposure, and detector-response normalization are not implemented.

From the repository root, run `root -l results/quickstart/run*/run*_EventEnergy.root` (after one quick-start run; if several exist, use the explicit path printed as `CUPID_RUN_DIRECTORY`), then enter at the ROOT prompt:

```cpp
auto* events = _file0->Get<TTree>("LMO0");
events->Print();
events->Draw("Edep>>hLMO0(200,0,1100)");
gPad->SaveAs("results/quickstart/LMO0-spectrum.png");
```

This plots the event-level LMO0 spectrum: keV on the horizontal axis and records per bin on the vertical axis. Exit with `.q`. To check energy agreement between recorders, pass both files:

```bash
python3 tests/check_root_records.py \
  results/quickstart/run*/run*_EventEnergy.root \
  results/quickstart/run*/run*_IncidentEnergy.root --events 1000 --run 0
```

The checker also supports historical combined schema 1/2/3 files; their original tree names and `DetectorName` branches remain unchanged. Existing data are not converted automatically.

## 8. Reproducibility and Run Records

For production simulations, preserve the ROOT files and logs, and retain enough information to recover:

- The code commit and working-tree status; save the diff and untracked inputs if changes are uncommitted.
- Geant4, ROOT, compiler, build options, and dataset versions, with input checksums where needed.
- The actual macros and submacros, geometry, materials, and physics parameters.
- The random engine, seeds/state, event and thread counts, run mode, command, and output identifier.

Each run log contains shared initialization/macro context followed by only that run's transport output and result summary. Empty runs retain a log even when no ROOT file is saved. Errors before the first run starts are reported on the console; no run directory exists yet.

**Recorded automatically:** Geant4/ROOT versions, command and working directory, resolvable static macro contents, gun parameters, random state at run start, requested/processed event counts, mode/threads, and output summaries. The current environment uses MixMaxRng.

**Still recorded manually:** commit/dirty status, complete build and dataset information, dynamic macros, interactive input, and external files. Prefer a clean commit and static macros, and do not modify source or inputs during a run. After completion, append the following information to the corresponding run log from the repository root, adjusting the output path:

```bash
{
  printf '\nCUPID_MANUAL_PROVENANCE_BEGIN\n'
  git rev-parse HEAD
  git status --short
  uname -sr
  c++ --version | head -1
  cmake --version | head -1
  root-config --version
  geant4-config --version
  env | sort | grep -E '^G4[A-Z0-9_]*DATA='
  grep -E 'CMAKE_BUILD_TYPE:|CMAKE_CXX_COMPILER:|CUPID_WITH_VIS:|Geant4_DIR:|ROOT_DIR:|CMAKE_PREFIX_PATH:' build/CMakeCache.txt
  printf 'CUPID_MANUAL_PROVENANCE_END\n'
} >> "results/quickstart/REPLACE_WITH_RUN_PREFIX/run.log"
```

Paths alone do not establish dataset versions: verify the versions and save SHA-256 checksums for external inputs. A dirty flag cannot reconstruct code; preserve diffs and untracked inputs for uncommitted versions. Also record dynamic macros, interactive input, and final parameters not fixed in the source.

To reproduce a run, check out the same commit, restore dependencies, data, and configuration, and execute the same macro in a new output directory with the same engine/seeds, mode, and thread count. There is no automatic replay tool or archive of per-event random states. Statistical consistency is the current goal; fixed seeds do not guarantee identical events or file bytes across versions, environments, or thread configurations.

## 9. Tests and Physics Validation

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

Compilation and minimal runs check usability. Geometry checks sample for overlaps. CTest covers sampling, materials, ancestry attribution, positive-deposit filtering, ROOT fields, and serial/multithreaded output. Passing sampled overlap checks does not prove every geometric detail is correct.

There are currently 21 configured CTest cases covering the supported framework behavior. The two tests specific to the removed generic serialization layer are no longer configured. All 21 tests passed on 2026-11-02 after simplifying the typed interfaces (93.14 seconds), including recorder selection, invalid-record rejection, ROOT column/value readback, serial/four-worker merging, empty/repeated runs and energy agreement. Historical validation details are retained in `history.log`. All 21 tests also passed after the output-naming/comment update (104.94 seconds). Output naming checks cover automatic/custom names, restoring automatic mode, repeated runs, shared worker names, invalid prefixes and collision protection. No acceptance benchmarks have yet been established for experimental spectra, efficiencies, or absolute background rates. New physics validation must document reference sources, event counts, statistical and systematic uncertainties, and acceptance tolerances agreed in advance. Software regression is not a substitute for physics validation.

## 10. Team Collaboration

Create a feature branch from the latest `main`, such as `feature/source-th232`, and keep each PR focused. Before committing, build, run affected CTest cases, and run `git diff --check`; run the full suite before merging. Describe behavior, validation, and limitations in the PR, and obtain review from at least one other member. Remote CI and branch protection are not configured in the current workflow, so checks are manual; state any unavailable test conditions explicitly.

- Before implementing a feature, inspect applicable `G4*` interfaces in the current Geant4 `source/` tree and prefer suitable existing functionality. Examples do not replace source inspection.
- Put headers in `include/` and implementations in matching `src/` directories. Use `fName` for members and lowercase-initial names for locals and parameters; keep interface and macro names stable.
- Group includes in this order: project headers, Geant4, other dependencies/standard library, with blank lines between groups.
- Use English comments for current purpose, usage, and algorithms. Explain configurable parameters at their setting points, including units, valid ranges and macro/CLI overrides; briefly explain key algorithms, ownership and thread ordering. Append change history to `history.log`. Replace obsolete code directly rather than retaining commented-out implementations by default.
- Make units, coordinates, thread ownership, and lifetimes explicit; avoid mutable state shared across events or threads without isolation.
- A new member should independently run the quick start in a clean environment and record installation steps, commands, timing, and results. This validation is still pending.

| Change | Required accompanying updates |
| --- | --- |
| Geometry, materials, default source parameters | Macros/configuration descriptions, geometry checks, affected regressions, and physics validation where needed |
| Physics list, cuts, nuclear data | Defaults, convergence checks, and physics-validation evidence, beyond successful execution |
| Recorders and ancestry attribution | Statistical definitions, energy accounting/attribution tests, zero-deposit and cross-detector cases |
| Output fields, units, or meaning | README output definitions, schema version, ROOT readback/merging tests, and compatibility policy |
| Public interfaces, command line, or macros | File-level comments, quick-start/usage examples, and corresponding tests |

## 11. Git and Data Management

Track source, build configuration, macros, small tests, and documentation. Ignore `build*/`, ROOT files, temporary logs, `output/`, and `results/`; `history.log` is the exception for logs. Supply installation paths through environment variables or local configuration, using the commands in Section 2. Do not commit personal absolute paths or credentials.

No Git LFS or centralized data repository policy is established yet. Before adding large inputs or reference datasets, agree on external storage and document retrieval, versions, checksums, and licensing. Do not add large simulation outputs directly to Git. The person responsible for each study must back up its production results separately.

## 12. Troubleshooting, Maintenance, and License

| Problem | Action |
| --- | --- |
| Geant4/ROOT not found | Check environment scripts, installation prefixes, and exact versions; see Section 3 for Vdt |
| Missing datasets | Install data matching the Geant4 version and reload `geant4.sh` |
| Visualization fails | Check WSLg/X11, Geant4 graphics support, and `CUPID_WITH_VIS`; visualization can be disabled for batch runs |
| Macro not found or edits ineffective | Run `macros/xxx.mac` from the root; check submacro paths and build-directory copies |
| No ROOT file | Check exit status and `run.log`; `file=none(empty)` indicates no saved records |
| Output location or permissions | Set `--output DIR` or `/cupid/output/directory DIR`; existing run names receive numeric suffixes |

Maintenance and feedback: [repository Issues](https://github.com/Loyh617/CUPID-CJPL/issues), owned by Loyh617; team responsibilities remain to be documented. No LICENSE file is provided yet, and no open-source license grant is declared. For now, cite the repository URL and commit used; the maintainer has yet to establish the formal license and citation format.
