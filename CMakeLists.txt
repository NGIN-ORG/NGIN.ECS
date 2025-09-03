cmake_minimum_required(VERSION 3.16)
project(NGIN.ECS VERSION 0.1 LANGUAGES CXX)

#-------------------------------------------------------------------------------
# Policies
#-------------------------------------------------------------------------------
cmake_policy(SET CMP0077 NEW)

#-------------------------------------------------------------------------------
# Options
#-------------------------------------------------------------------------------
option(NGIN_ECS_BUILD_TESTS "Build NGIN.ECS tests" ON)
option(NGIN_ECS_BUILD_EXAMPLES "Build NGIN.ECS examples" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


set(NGIN_BASE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NGIN_BASE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(NGIN_BASE_DEVELOPMENT_MODE OFF CACHE BOOL "" FORCE)
set(NGIN_BASE_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
#-------------------------------------------------------------------------------
# Locate NGIN.Base
#-------------------------------------------------------------------------------
if(NOT TARGET NGIN::Base)
  find_package(NGINBase CONFIG QUIET)
  if(NOT NGINBase_FOUND)
    set(_base_dir "${CMAKE_CURRENT_LIST_DIR}/../NGIN.Base")
    if(EXISTS "${_base_dir}/CMakeLists.txt")
      message(STATUS "NGIN.ECS: using sibling NGIN.Base from ${_base_dir}")
      set(NGIN_BASE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
      set(NGIN_BASE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
      set(NGIN_BASE_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
      add_subdirectory("${_base_dir}" _ngin_base EXCLUDE_FROM_ALL)
    elseif(DEFINED NGIN_BASE_SOURCE_DIR)
      message(STATUS "NGIN.ECS: using NGIN.Base from ${NGIN_BASE_SOURCE_DIR}")
      set(NGIN_BASE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
      set(NGIN_BASE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
      set(NGIN_BASE_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
      add_subdirectory("${NGIN_BASE_SOURCE_DIR}" _ngin_base EXCLUDE_FROM_ALL)
    else()
      message(FATAL_ERROR "NGIN.Base not found. Install NGINBase or set NGIN_BASE_SOURCE_DIR to the source path.")
    endif()
  endif()
endif()

#-------------------------------------------------------------------------------
# Library Definition (shared/static controlled by BUILD_SHARED_LIBS)
#-------------------------------------------------------------------------------
add_library(NGIN.ECS
  src/ECS.cpp
  src/Entity.cpp
)

target_compile_features(NGIN.ECS PUBLIC cxx_std_23)

target_include_directories(NGIN.ECS
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(NGIN.ECS
  PUBLIC
    NGIN::Base
)

# Platform and export macros
target_compile_definitions(NGIN.ECS
  PUBLIC
    $<$<STREQUAL:$<PLATFORM_ID>,Windows>:NGIN_PLATFORM=\"Windows\">
    $<$<STREQUAL:$<PLATFORM_ID>,Darwin>:NGIN_PLATFORM=\"macOS\">
    $<$<STREQUAL:$<PLATFORM_ID>,Linux>:NGIN_PLATFORM=\"Linux\">
    $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,STATIC_LIBRARY>:NGIN_ECS_STATIC=1>
    $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:NGIN_ECS_SHARED=1>
  PRIVATE
    $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:NGIN_ECS_EXPORTS=1>
)

add_library(NGIN::ECS ALIAS NGIN.ECS)

# Add -latomic if needed
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_link_libraries(NGIN.ECS PUBLIC atomic)
endif()

#-------------------------------------------------------------------------------
# Installation
#-------------------------------------------------------------------------------
include(GNUInstallDirs)

install(
  DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(TARGETS NGIN.ECS EXPORT NGINECSTargets)

install(
  EXPORT NGINECSTargets
  FILE NGINECSTargets.cmake
  NAMESPACE NGIN::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NGINECS
)

# Package config
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/NGINECSConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/NGINECSConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/NGINECSConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NGINECS
)

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/NGINECSConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/NGINECSConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NGINECS
)

#-------------------------------------------------------------------------------
# Tests
#-------------------------------------------------------------------------------
if(NGIN_ECS_BUILD_TESTS)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()

#-------------------------------------------------------------------------------
# Examples
#-------------------------------------------------------------------------------
if(NGIN_ECS_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
