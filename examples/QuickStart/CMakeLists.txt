cmake_minimum_required(VERSION 3.16)
project(ECSQuickStart LANGUAGES CXX)

add_executable(ECSQuickStart main.cpp)
target_link_libraries(ECSQuickStart PRIVATE NGIN::ECS)
target_compile_features(ECSQuickStart PRIVATE cxx_std_23)

