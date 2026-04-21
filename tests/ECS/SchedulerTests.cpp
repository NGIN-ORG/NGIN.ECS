/// @file SchedulerTests.cpp
/// @brief Typed system params, stage planning, and exclusive execution.

#include <boost/ut.hpp>

#include <NGIN/ECS/Scheduler.hpp>
#include <NGIN/ECS/Query.hpp>

using namespace boost::ut;

namespace
{
    struct A
    {
        int value;
    };

    struct Tag
    {
    };
}

suite<"NGIN::ECS::Scheduler"> schedSuite = [] {
  "Typed_Params_Derive_WriteRead_Stages"_test = [] {
    NGIN::ECS::World     world;
    NGIN::ECS::Scheduler scheduler;

    (void)world.Spawn(A{1});
    std::vector<int> order;

    auto writer = NGIN::ECS::MakeSystem("Writer", [&](NGIN::ECS::Query<NGIN::ECS::Write<A>>& query) {
      order.push_back(1);
      query.ForEach([&](const NGIN::ECS::RowView& row) {
        row.Write<A>().value += 5;
        row.MarkChanged<A>();
      });
    });

    auto reader = NGIN::ECS::MakeSystem("Reader", [&](NGIN::ECS::Query<NGIN::ECS::Read<A>>& query) {
      order.push_back(2);
      NGIN::UIntSize seen = 0;
      query.ForEach([&](const NGIN::ECS::RowView& row) {
        ++seen;
        expect(row.Read<A>().value == 6_i);
      });
      expect(eq(seen, 1_u));
    });

    scheduler.Register(writer);
    scheduler.Register(reader);
    scheduler.Build();

    expect(eq(scheduler.StageCount(), 2_u));
    scheduler.Run(world);
    expect(order.size() == 2_u);
    expect(order[0] == 1_i && order[1] == 2_i);
  };

  "Commands_Param_Forces_Barrier_And_Flush"_test = [] {
    NGIN::ECS::World     world;
    NGIN::ECS::Scheduler scheduler;

    auto spawner = NGIN::ECS::MakeSystem("Spawner", [&](NGIN::ECS::Commands& commands) {
      for (int i = 0; i < 10; ++i)
      {
          commands.Spawn(Tag{});
      }
    });

    auto counter = NGIN::ECS::MakeSystem("Counter", [&](NGIN::ECS::Query<NGIN::ECS::Read<Tag>>& query) {
      NGIN::UIntSize count = 0;
      query.ForEach([&](const NGIN::ECS::RowView&) { ++count; });
      expect(eq(count, 10_u));
    });

    scheduler.Register(spawner);
    scheduler.Register(counter);
    scheduler.Build();

    expect(eq(scheduler.StageCount(), 2_u));
    scheduler.Run(world);
  };

  "Exclusive_System_Runs_In_Its_Own_Stage"_test = [] {
    NGIN::ECS::World     world;
    NGIN::ECS::Scheduler scheduler;

    (void)world.Spawn(A{1});
    std::vector<int> order;

    auto reader = NGIN::ECS::MakeSystem("Reader", [&](NGIN::ECS::Query<NGIN::ECS::Read<A>>& query) {
      order.push_back(1);
      NGIN::UIntSize count = 0;
      query.ForEach([&](const NGIN::ECS::RowView&) { ++count; });
      expect(eq(count, 1_u));
    });

    auto exclusive = NGIN::ECS::MakeExclusiveSystem("Exclusive", [&](NGIN::ECS::ExclusiveWorld worldAccess) {
      order.push_back(2);
      worldAccess->Clear();
    });

    auto counter = NGIN::ECS::MakeSystem("Counter", [&](NGIN::ECS::Query<NGIN::ECS::Read<A>>& query) {
      order.push_back(3);
      NGIN::UIntSize count = 0;
      query.ForEach([&](const NGIN::ECS::RowView&) { ++count; });
      expect(eq(count, 0_u));
    });

    scheduler.Register(reader);
    scheduler.Register(exclusive);
    scheduler.Register(counter);
    scheduler.Build();

    expect(eq(scheduler.StageCount(), 3_u));
    scheduler.Run(world);
    expect(order.size() == 3_u);
    expect(order[0] == 1_i && order[1] == 2_i && order[2] == 3_i);
  };
};
