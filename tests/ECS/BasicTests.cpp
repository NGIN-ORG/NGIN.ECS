/// @file BasicTests.cpp
/// @brief Basic smoke tests for NGIN.ECS skeleton.

#include <boost/ut.hpp>

#include <NGIN/ECS/ECS.hpp>

using namespace boost::ut;

suite<"NGIN::ECS"> ecsSuite = [] {
  "LibraryName"_test = [] {
    expect(eq(NGIN::ECS::LibraryName(), std::string_view{"NGIN.ECS"}));
  };

  "ParseInt_Valid"_test = [] {
    expect(eq(NGIN::ECS::ParseInt("-7"), -7));
  };

  "ParseInt_Invalid_Throws"_test = [] {
    expect(throws<std::invalid_argument>([] { (void)NGIN::ECS::ParseInt("oops"); }));
  };
};

