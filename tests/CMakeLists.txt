cmake_minimum_required(VERSION 3.16)
project(NGINECSTests LANGUAGES CXX)

include(${CMAKE_SOURCE_DIR}/cmake/CPM.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/BoostUT.cmake)

CPMAddPackage(
  NAME ut
  VERSION 2.3.1
  GITHUB_REPOSITORY boost-ext/ut
  OPTIONS
    "BOOST_UT_DISABLE_MODULE ON"
    "BOOST_UT_BUILD_EXAMPLES OFF"
    "BOOST_UT_BUILD_TESTS OFF"
)

add_library(ngin_ecs_ut_config INTERFACE)
target_link_libraries(ngin_ecs_ut_config INTERFACE Boost::ut NGIN::ECS)
target_compile_features(ngin_ecs_ut_config INTERFACE cxx_std_23)

set(MAIN_SRC "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

# --- Root-level test sources ---
file(GLOB ROOT_TEST_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(REMOVE_ITEM ROOT_TEST_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

foreach(test_src IN LISTS ROOT_TEST_SRCS)
  get_filename_component(test_name "${test_src}" NAME_WE)
  set(exe_name "ECS_${test_name}")
  add_executable(${exe_name} ${MAIN_SRC} ${test_src})
  target_link_libraries(${exe_name} PRIVATE ngin_ecs_ut_config)
  set_target_properties(${exe_name} PROPERTIES FOLDER "Tests")
  source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${test_src})
  discover_boost_ut_test(${exe_name} TEST_SOURCE ${test_src})
endforeach()

# --- Subdirectory test sources ---
file(GLOB CHILDREN RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*")
foreach(child IN LISTS CHILDREN)
  if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${child}")
    file(GLOB SUBDIR_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${child}/*.cpp")
    foreach(test_src IN LISTS SUBDIR_SRCS)
      get_filename_component(test_name "${test_src}" NAME_WE)
      string(REPLACE "-" "_" child_safe "${child}")
      set(exe_name "${child_safe}_${test_name}")
      add_executable(${exe_name} ${MAIN_SRC} ${test_src})
      target_link_libraries(${exe_name} PRIVATE ngin_ecs_ut_config)
      set_target_properties(${exe_name} PROPERTIES FOLDER "Tests")
      source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${test_src})
      discover_boost_ut_test(${exe_name} TEST_SOURCE ${test_src})
    endforeach()
  endif()
endforeach()
