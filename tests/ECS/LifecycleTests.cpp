/// @file LifecycleTests.cpp
/// @brief Non-trivial and move-only component lifetime coverage.

#include <boost/ut.hpp>

#include <NGIN/ECS/World.hpp>
#include <NGIN/ECS/Commands.hpp>
#include <NGIN/ECS/Query.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace boost::ut;

namespace
{
    struct Tag
    {
    };

    struct MoveOnly
    {
        static inline int Alive = 0;

        explicit MoveOnly(int input)
            : Value(std::make_unique<int>(input))
        {
            ++Alive;
        }

        MoveOnly(MoveOnly&& other) noexcept
            : Value(std::move(other.Value))
        {
            ++Alive;
        }

        MoveOnly& operator=(MoveOnly&& other) noexcept
        {
            Value = std::move(other.Value);
            return *this;
        }

        MoveOnly(const MoveOnly&)            = delete;
        MoveOnly& operator=(const MoveOnly&) = delete;

        ~MoveOnly()
        {
            --Alive;
        }

        std::unique_ptr<int> Value;
    };
}

suite<"NGIN::ECS::Lifecycle"> lifecycleSuite = [] {
  "String_Vector_And_MoveOnly_Survive_Migration_And_Clear"_test = [] {
    expect(eq(MoveOnly::Alive, 0_i));

    {
        NGIN::ECS::World world;
        const auto entity = world.Spawn(std::string{"alpha"}, std::vector<int>{1, 2, 3}, MoveOnly{7});

        expect(world.Get<std::string>(entity) == std::string{"alpha"});
        expect(eq(world.Get<std::vector<int>>(entity).size(), std::size_t{3}));
        expect(*world.Get<MoveOnly>(entity).Value == 7_i);

        world.Add<Tag>(entity, Tag{});
        expect(world.Has<Tag>(entity));
        expect(world.Get<std::string>(entity) == std::string{"alpha"});

        expect(world.Remove<Tag>(entity));
        expect(!world.Has<Tag>(entity));

        world.Set<std::string>(entity, std::string{"beta"});
        expect(world.Get<std::string>(entity) == std::string{"beta"});

        auto& values = world.GetMut<std::vector<int>>(entity);
        values.push_back(4);
        world.MarkChanged<std::vector<int>>(entity);

        NGIN::ECS::Commands commands;
        commands.Spawn(std::string{"gamma"}, std::vector<int>{9}, MoveOnly{11});
        commands.Flush(world);

        NGIN::UIntSize rows = 0;
        NGIN::ECS::Query<NGIN::ECS::Read<std::string>, NGIN::ECS::Read<MoveOnly>> query {world};
        query.ForEach([&](const NGIN::ECS::RowView& row) {
          ++rows;
          expect(row.Read<std::string>().size() >= std::size_t {4});
          expect(row.Read<MoveOnly>().Value != nullptr);
        });
        expect(eq(rows, 2_u));

        world.Clear();
        expect(eq(world.AliveCount(), 0ULL));
    }

    expect(eq(MoveOnly::Alive, 0_i));
  };
};
