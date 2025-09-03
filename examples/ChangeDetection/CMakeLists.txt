cmake_minimum_required(VERSION 3.16)
project(ECSChangeDetection LANGUAGES CXX)

add_executable(ECSChangeDetection main.cpp)
target_link_libraries(ECSChangeDetection PRIVATE NGIN::ECS)
target_compile_features(ECSChangeDetection PRIVATE cxx_std_23)

