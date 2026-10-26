cmake_minimum_required(VERSION 3.16)
project(CupidCJPL VERSION 0.1.0 LANGUAGES CXX)

option(CUPID_WITH_VIS "Build the interactive UI and visualization" ON)
if(CUPID_WITH_VIS)
  find_package(Geant4 11.4.1 EXACT REQUIRED multithreaded ui_all vis_all)
else()
  find_package(Geant4 11.4.1 EXACT REQUIRED multithreaded)
endif()
include(${Geant4_USE_FILE})

add_executable(cupid-sim
  main.cc
  src/DetectorConstruction.cc
  src/LMOSensitiveDetector.cc
  src/DetectorHit.cc
  src/record/EventEnergyRecorder.cc
  src/record/IncidentEnergyRecorder.cc
  src/record/TrackAncestry.cc
  src/ActionInitialization.cc
  src/PrimaryGeneratorAction.cc
  src/generator/toolkit/sampler/PointSampler.cc
  src/generator/toolkit/sampler/RejectionVolumeSampler.cc
  src/generator/toolkit/sampler/DirectVolumeSampler.cc
  src/generator/toolkit/sampler/DirectSurfaceSampler.cc
  src/generator/toolkit/NuclideEmitter.cc
  src/generator/calibration/th232/Th232Generator.cc
  src/generator/calibration/co56/Co56Generator.cc
  src/generator/lmo_signal/double_beta_decay/TwoNuDoubleBetaDecayGenerator.cc
  src/generator/lmo_signal/double_beta_decay/ZeroNuDoubleBetaDecayGenerator.cc
  src/generator/GeneratorManager.cc
  src/generator/GeneratorMessenger.cc
  src/PhysicsList.cc
  src/RunAction.cc
  src/EventAction.cc
  src/SteppingAction.cc
  src/TrackingAction.cc
  src/StackingAction.cc
  src/geometry/GeometryManager.cc
  src/geometry/GeometryMessenger.cc
  src/geometry/Cryostat.cc
  src/geometry/InnerShielding.cc
  src/geometry/LMOArray.cc
  src/materials/Materials.cc
)
target_compile_features(cupid-sim PRIVATE cxx_std_17)
set_target_properties(cupid-sim PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(cupid-sim PRIVATE include)
target_link_libraries(cupid-sim PRIVATE ${Geant4_LIBRARIES})
target_compile_definitions(cupid-sim PRIVATE
  CUPID_WITH_VIS=$<BOOL:${CUPID_WITH_VIS}>
  CUPID_MACRO_DIR="${CMAKE_CURRENT_BINARY_DIR}/macros"
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(cupid-sim PRIVATE -Wall -Wextra -Wpedantic)
endif()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/macros")
foreach(name geometry run lmo_only check_geometry vis draw render)
  configure_file(macros/${name}.mac macros/${name}.mac COPYONLY)
endforeach()

include(CTest)
if(BUILD_TESTING)
  add_executable(check-materials tests/check_materials.cc src/materials/Materials.cc)
  target_compile_features(check-materials PRIVATE cxx_std_17)
  target_include_directories(check-materials PRIVATE include)
  target_link_libraries(check-materials PRIVATE ${Geant4_LIBRARIES})
  add_test(NAME material-checks COMMAND check-materials)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  if(CUPID_WITH_VIS)
    set(test_vis_argument --with-vis)
  endif()
  add_test(NAME framework-checks
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_framework.py
      $<TARGET_FILE:cupid-sim> ${CMAKE_CURRENT_BINARY_DIR}/test-results ${test_vis_argument})
  set_tests_properties(framework-checks PROPERTIES TIMEOUT 240)
  add_test(NAME physics-checks
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_physics.py
      $<TARGET_FILE:cupid-sim> ${CMAKE_CURRENT_BINARY_DIR}/physics-test-results)
  set_tests_properties(physics-checks PROPERTIES TIMEOUT 240)
  add_test(NAME generator-checks
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_generators.py
      $<TARGET_FILE:cupid-sim> ${CMAKE_CURRENT_BINARY_DIR}/generator-test-results)
  set_tests_properties(generator-checks PROPERTIES TIMEOUT 240)
  add_executable(check-vertex-sampler
    tests/check_vertex_sampler.cc
    src/generator/toolkit/sampler/PointSampler.cc
    src/generator/toolkit/sampler/RejectionVolumeSampler.cc
    src/geometry/GeometryManager.cc src/geometry/GeometryMessenger.cc
    src/geometry/Cryostat.cc src/geometry/InnerShielding.cc src/geometry/LMOArray.cc
    src/materials/Materials.cc)
  target_compile_features(check-vertex-sampler PRIVATE cxx_std_17)
  target_include_directories(check-vertex-sampler PRIVATE include)
  target_link_libraries(check-vertex-sampler PRIVATE ${Geant4_LIBRARIES})
  add_test(NAME vertex-sampler-checks COMMAND check-vertex-sampler)
  add_executable(check-direct-samplers
    tests/check_direct_samplers.cc
    src/generator/toolkit/sampler/DirectVolumeSampler.cc
    src/generator/toolkit/sampler/DirectSurfaceSampler.cc)
  target_compile_features(check-direct-samplers PRIVATE cxx_std_17)
  target_include_directories(check-direct-samplers PRIVATE include)
  target_link_libraries(check-direct-samplers PRIVATE ${Geant4_LIBRARIES})
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(check-direct-samplers PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  add_test(NAME direct-sampler-checks COMMAND check-direct-samplers)
  add_executable(check-lmo-sd
    tests/check_lmo_sd.cc
    src/EventAction.cc src/TrackingAction.cc
    src/DetectorConstruction.cc src/PhysicsList.cc
    src/LMOSensitiveDetector.cc src/DetectorHit.cc
    src/record/EventEnergyRecorder.cc src/record/IncidentEnergyRecorder.cc
    src/record/TrackAncestry.cc
    src/geometry/GeometryManager.cc src/geometry/GeometryMessenger.cc
    src/geometry/Cryostat.cc src/geometry/InnerShielding.cc src/geometry/LMOArray.cc
    src/materials/Materials.cc)
  target_compile_features(check-lmo-sd PRIVATE cxx_std_17)
  target_include_directories(check-lmo-sd PRIVATE include)
  target_link_libraries(check-lmo-sd PRIVATE ${Geant4_LIBRARIES})
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(check-lmo-sd PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  add_executable(check-energy-records
    tests/check_energy_records.cc src/DetectorHit.cc src/EventAction.cc
    src/record/EventEnergyRecorder.cc src/record/IncidentEnergyRecorder.cc
    src/record/TrackAncestry.cc)
  target_compile_features(check-energy-records PRIVATE cxx_std_17)
  target_include_directories(check-energy-records PRIVATE include)
  target_link_libraries(check-energy-records PRIVATE ${Geant4_LIBRARIES})
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(check-energy-records PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  add_test(NAME energy-record-checks COMMAND check-energy-records)
  foreach(mode serial mt)
    foreach(count 0 1 2)
      add_test(NAME lmo-sd-${mode}-${count} COMMAND check-lmo-sd ${mode} ${count})
      set_tests_properties(lmo-sd-${mode}-${count} PROPERTIES TIMEOUT 120)
    endforeach()
  endforeach()
endif()

