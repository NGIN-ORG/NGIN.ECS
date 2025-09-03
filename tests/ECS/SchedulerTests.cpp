/// @file SchedulerTests.cpp
/// @brief Tests for system DAG scheduling and stage barriers (command flush).

#include <boost/ut.hpp>

#include <NGIN/ECS/Scheduler.hpp>
#include <NGIN/ECS/Query.hpp>

using namespace boost::ut;

namespace
{
    struct A { int v; };
    struct B { int v; };
    struct Tag {};
}

suite<"NGIN::ECS::Scheduler"> schedSuite = [] {
  "Topo_Order_WriteRead"_test = [] {
    NGIN::ECS::World     world;
    NGIN::ECS::Scheduler sched;

    std::vector<int> order;
    auto s1 = NGIN::ECS::MakeSystem<NGIN::ECS::Write<A>>("S1", [&](NGIN::ECS::World& w, NGIN::ECS::Commands&) {
      (void)w; order.push_back(1);
    });
    auto s2 = NGIN::ECS::MakeSystem<NGIN::ECS::Read<A>>("S2", [&](NGIN::ECS::World& w, NGIN::ECS::Commands&) {
      (void)w; order.push_back(2);
    });
    sched.Register(s1);
    sched.Register(s2);
    sched.Build();
    sched.Run(world);
    expect(order.size() == 2_u);
    expect(order[0] == 1_i && order[1] == 2_i);
    expect(sched.StageCount() >= 1_u);
  };

  "Barrier_Flush_Applies_Spawn_Tag"_test = [] {
    NGIN::ECS::World     world;
    NGIN::ECS::Scheduler sched;

    auto spawnSys = NGIN::ECS::MakeSystem<NGIN::ECS::Write<Tag>>("Spawn", [&](NGIN::ECS::World& w, NGIN::ECS::Commands& cmd) {
      for (int i = 0; i < 10; ++i)
        cmd.Spawn(Tag{});
    });
    auto readSys = NGIN::ECS::MakeSystem<NGIN::ECS::Read<Tag>>("Read", [&](NGIN::ECS::World& w, NGIN::ECS::Commands&) {
      NGIN::ECS::Query<NGIN::ECS::Read<Tag>> q{w};
      NGIN::UIntSize count = 0;
      q.for_chunks([&](const NGIN::ECS::ChunkView& ch){ count += (ch.end() - ch.begin()); });
      expect(count == 10_u);
    });
    sched.Register(spawnSys);
    sched.Register(readSys);
    sched.Build();
    sched.Run(world);
  };
};

