cmake_minimum_required(VERSION 3.9)

project("AutoYAML"
        VERSION 0.1
        LANGUAGES CXX)

if (NOT CMAKE_CXX_COMPILER_ID STREQUAL Clang)
  message(FATAL_ERROR "CMAKE_CXX_COMPILER must be Clang")
endif()

find_package(LLVM REQUIRED CONFIG)

add_executable(autoyaml AutoYaml.cpp)

target_compile_definitions(autoyaml PRIVATE ${LLVM_DEFINITIONS})
target_include_directories(autoyaml PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(autoyaml PRIVATE clang-cpp)
target_compile_features(autoyaml PRIVATE cxx_std_17)

llvm_config(autoyaml USE_SHARED option)
