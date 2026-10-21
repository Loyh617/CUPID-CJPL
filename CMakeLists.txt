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
  src/ActionInitialization.cc
  src/PrimaryGeneratorAction.cc
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
  src/geometry/CrystalArray.cc
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
foreach(name geometry run crystal_only check_geometry vis draw render)
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
endif()

