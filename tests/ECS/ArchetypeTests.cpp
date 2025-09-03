/// @file ArchetypeTests.cpp
/// @brief Signature canonicalization and hashing tests.

#include <boost/ut.hpp>

#include <NGIN/ECS/Archetype.hpp>

using namespace boost::ut;

namespace
{
    struct C1 { int x; };
    struct C2 { float y; };
    struct Tag {};
}

suite<"NGIN::ECS::Archetype"> archSuite = [] {
  "Signature_Canonicalization"_test = [] {
    NGIN::Containers::Vector<NGIN::ECS::TypeId> a;
    a.PushBack(NGIN::ECS::GetTypeId<C2>());
    a.PushBack(NGIN::ECS::GetTypeId<C1>());
    a.PushBack(NGIN::ECS::GetTypeId<Tag>());

    NGIN::Containers::Vector<NGIN::ECS::TypeId> b;
    b.PushBack(NGIN::ECS::GetTypeId<Tag>());
    b.PushBack(NGIN::ECS::GetTypeId<C1>());
    b.PushBack(NGIN::ECS::GetTypeId<C2>());

    const auto s1 = NGIN::ECS::ArchetypeSignature::FromUnordered(std::move(a));
    const auto s2 = NGIN::ECS::ArchetypeSignature::FromUnordered(std::move(b));
    expect(s1 == s2);
    expect(eq(s1.Hash, s2.Hash));
  };
};

