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

file(GLOB ROOT_TEST_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(REMOVE_ITEM ROOT_TEST_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
if(ROOT_TEST_SRCS)
  add_executable(ECSRoot ${MAIN_SRC} ${ROOT_TEST_SRCS})
  target_link_libraries(ECSRoot PRIVATE ngin_ecs_ut_config)
  set_target_properties(ECSRoot PROPERTIES FOLDER "Tests")
  source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ROOT_TEST_SRCS})
  discover_boost_ut_test(ECSRoot)
endif()

file(GLOB CHILDREN RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*")
foreach(child IN LISTS CHILDREN)
  if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${child}")
    file(GLOB_RECURSE SUBDIR_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${child}/*.cpp")
    if(SUBDIR_SRCS)
      string(REPLACE "-" "_" SUBDIR_TARGET_SAFE "${child}")
      add_executable(${SUBDIR_TARGET_SAFE} ${MAIN_SRC} ${SUBDIR_SRCS})
      target_link_libraries(${SUBDIR_TARGET_SAFE} PRIVATE ngin_ecs_ut_config)
      set_target_properties(${SUBDIR_TARGET_SAFE} PROPERTIES FOLDER "Tests")
      source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SUBDIR_SRCS})
      discover_boost_ut_test(${SUBDIR_TARGET_SAFE})
    endif()
  endif()
endforeach()

