cmake_minimum_required(VERSION 3.9)

project("AutoYAML"
        VERSION 0.1
        LANGUAGES CXX)

include(CTest)

# project structure
set(AutoYAML_src AutoYAML.cpp)
set(AutoYAML_bin autoyaml)
set(AutoYAML_example_in AutoYAML_example.h)
set(AutoYAML_example_out AutoYAML_example.AutoYAML.h)
set(AutoYAML_example_out_dir "${CMAKE_CURRENT_BINARY_DIR}")
set(AutoYAML_test_src AutoYAML_test.cpp)

# options
option(AUTO_YAML_INSTALL "Generate AutoYAML install target" ON)
option(AUTO_YAML_BUILD_EXAMPLE "Build AutoYAML example" ON)
option(AUTO_YAML_BUILD_TEST "Build AutoYAML test" ON)

# dependencies

# Clang
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/ClangSetup)

include(ClangSetup)

# yaml-cpp
add_subdirectory(yaml-cpp EXCLUDE_FROM_ALL)

# build
add_executable(${AutoYAML_bin} "${AutoYAML_src}")
target_compile_features(${AutoYAML_bin} PRIVATE cxx_std_17)
target_compile_definitions(${AutoYAML_bin} PRIVATE -DCLANG_INCLUDE_PATHS="${CLANG_INCLUDE_PATHS}")
target_link_libraries(${AutoYAML_bin} PRIVATE clang-cpp)
llvm_config(${AutoYAML_bin} USE_SHARED option)

# install
if (AUTO_YAML_INSTALL)
  install(TARGETS ${AutoYAML_bin} DESTINATION bin)
endif()

# example
if (AUTO_YAML_BUILD_EXAMPLE)
  add_custom_command(
    OUTPUT "${AutoYAML_example_out_dir}/${AutoYAML_example_out}"
    COMMAND $<TARGET_FILE:${AutoYAML_bin}>
            --out-dir "${AutoYAML_example_out_dir}"
            "${AutoYAML_example_in}"
            --
    DEPENDS "${AutoYAML_src}" "${AutoYAML_example_in}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

  add_library(AutoYAML_example INTERFACE)
  target_sources(AutoYAML_example PRIVATE "${AutoYAML_example_out}")
  target_include_directories(AutoYAML_example INTERFACE "${AutoYAML_example_out_dir}")
  target_link_libraries(AutoYAML_example INTERFACE yaml-cpp::yaml-cpp)
endif()

# test
if (AUTO_YAML_BUILD_TEST AND BUILD_TESTING)
  if (NOT AUTO_YAML_BUILD_EXAMPLE)
    message(FATAL_ERROR "AUTO_YAML_BUILD_EXAMPLE must be enabled in order to build test")
  endif()

  add_executable(AutoYAML_test "${AutoYAML_test_src}")
  target_link_libraries(AutoYAML_test PRIVATE AutoYAML_example)
endif()
