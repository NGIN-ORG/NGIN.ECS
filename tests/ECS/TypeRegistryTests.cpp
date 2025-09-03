/// @file TypeRegistryTests.cpp
/// @brief Tests for component type description and TypeId.

#include <boost/ut.hpp>

#include <NGIN/ECS/TypeRegistry.hpp>

using namespace boost::ut;

namespace
{
    struct PODType
    {
        int a;
        float b;
    };
    struct NonPOD
    {
        NonPOD() {}
        ~NonPOD() {}
        NonPOD(const NonPOD&) {}
        NonPOD& operator=(const NonPOD&) { return *this; }
        int x;
    };
    struct TagType {};
}

suite<"NGIN::ECS::TypeRegistry"> typeSuite = [] {
  "TypeId_Unique"_test = [] {
    const auto id1 = NGIN::ECS::GetTypeId<PODType>();
    const auto id2 = NGIN::ECS::GetTypeId<NonPOD>();
    expect(id1 != id2);
  };

  "Describe_POD_NonPOD_Tag"_test = [] {
    const auto i1 = NGIN::ECS::DescribeComponent<PODType>();
    expect(i1.IsPOD);
    expect(!i1.IsEmpty);
    expect(eq(i1.Size, sizeof(PODType)));
    expect(eq(i1.Align, alignof(PODType)));

    const auto i2 = NGIN::ECS::DescribeComponent<NonPOD>();
    expect(!i2.IsPOD);
    expect(!i2.IsEmpty);

    const auto tag = NGIN::ECS::DescribeComponent<TagType>();
    expect(tag.IsEmpty);
    expect(eq(tag.Size, sizeof(TagType)));
  };
};

