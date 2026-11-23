// Compile-failure samples of the relationship pair (kernel DI-KS 29;
// declaration v8-arch-gyo2-kernel-relationship 5.2 and 12.2). Compiled at
// configure time once per variant by tests/common/kernel/CMakeLists.txt, with
// the judgement of tests/common/support/ExpectStaticAssert.cmake (a static
// assertion failing with the variant's own marker on the same line):
//   GYO_KERNEL_SAMPLE_ACCEPTED   every entry that may touch ChildOf or Children,
//                                used as allowed; must compile (the control).
// One variant per refused entry; ChildOf is not written in place, Children is
// kept by the kernel (only a RestoreSet puts one back):
//   REL_WRITE_TERM_CHILDOF, REL_WRITE_TERM_CHILDREN        Query<Write<...>>
//   REL_WORLD_GET_MUT_CHILDOF, REL_WORLD_GET_MUT_CHILDREN  World::GetMut
//   REL_EXCLUSIVE_MODIFY_CHILDOF, REL_EXCLUSIVE_MODIFY_CHILDREN  ExclusiveWorld::Modify
//   REL_COMMANDS_INSERT, REL_COMMANDS_REMOVE               Commands with Children
//   REL_COMMANDS_FOR_INSERT, REL_COMMANDS_FOR_REMOVE       CommandsFor with Children
//   REL_SPAWN, REL_SPAWN_WITH_ID                           a bundle with Children
//   REL_EXCLUSIVE_INSERT, REL_EXCLUSIVE_REMOVE, REL_EXCLUSIVE_SPAWN, REL_EXCLUSIVE_SPAWN_WITH_ID
//   REL_WORLD_INSERT, REL_WORLD_REMOVE                     the Host's direct puts of Children
//   REL_INITIAL_VALUE                                      Children as an initial value target
//   REL_DIAGNOSTICS                                        a System writing RejectedRelationship
#include "engine/kernel/Kernel.hpp"

namespace {

using namespace GYO::Kernel;

struct SampleItem {
    int v;
};
inline Children SampleChildren(const SampleItem&) { return Children{}; }

#if defined(GYO_KERNEL_SAMPLE_ACCEPTED)
SystemResult Reads(Query<Read<ChildOf>, With<Children>> of, Query<Read<Children>, Changed<Children>> children) {
    (void)of.Count();
    (void)children.Count();
    return {};
}
SystemResult Writes(Commands commands, CommandsFor<Inserts<ChildOf>, Removes<ChildOf>> typed,
                    SpawnWithId<Bundle<ChildOf>> spawn) {
    commands.Insert(1, ChildOf{2});
    commands.Remove<ChildOf>(1);
    typed.Insert(1, ChildOf{2});
    typed.Remove<ChildOf>(1);
    spawn.New(3, ChildOf{2});
    return {};
}
SystemResult Exclusive(ExclusiveWorld<> world) {
    (void)world.Insert(1, ChildOf{2});
    (void)world.Remove<ChildOf>(1);
    world.ReadColumn<Children>([](EntityId, ChangeStamp, ChangeStamp, const Children&) {});
    RestoreSet set;
    set.Column<ChildOf>({});
    set.Column<Children>({});
    return world.Restore(std::move(set));
}
[[maybe_unused]] void Host(World& world) {
    world.Insert(1, ChildOf{2});
    (void)world.Remove<ChildOf>(1);
    (void)world.Get<Children>(2);
}
#elif defined(REL_WRITE_TERM_CHILDOF)
SystemResult Bad(Query<Write<ChildOf>> q) { (void)q.Count(); return {}; }
#elif defined(REL_WRITE_TERM_CHILDREN)
SystemResult Bad(Query<Write<Children>> q) { (void)q.Count(); return {}; }
#elif defined(REL_WORLD_GET_MUT_CHILDOF)
[[maybe_unused]] void Bad(World& world) { (void)world.GetMut<ChildOf>(1); }
#elif defined(REL_WORLD_GET_MUT_CHILDREN)
[[maybe_unused]] void Bad(World& world) { (void)world.GetMut<Children>(1); }
#elif defined(REL_EXCLUSIVE_MODIFY_CHILDOF)
SystemResult Bad(ExclusiveWorld<> world) { (void)world.Modify<ChildOf>(1, [](ChildOf&) {}); return {}; }
#elif defined(REL_EXCLUSIVE_MODIFY_CHILDREN)
SystemResult Bad(ExclusiveWorld<> world) { (void)world.Modify<Children>(1, [](Children&) {}); return {}; }
#elif defined(REL_COMMANDS_INSERT)
SystemResult Bad(Commands commands) { commands.Insert(1, Children{}); return {}; }
#elif defined(REL_COMMANDS_REMOVE)
SystemResult Bad(Commands commands) { commands.Remove<Children>(1); return {}; }
#elif defined(REL_COMMANDS_FOR_INSERT)
SystemResult Bad(CommandsFor<Inserts<Children>, Removes<>> typed) { (void)sizeof(typed); return {}; }
#elif defined(REL_COMMANDS_FOR_REMOVE)
SystemResult Bad(CommandsFor<Inserts<>, Removes<Children>> typed) { (void)sizeof(typed); return {}; }
#elif defined(REL_SPAWN)
SystemResult Bad(Spawn<Bundle<Children>> spawn) { (void)sizeof(spawn); return {}; }
#elif defined(REL_SPAWN_WITH_ID)
SystemResult Bad(SpawnWithId<Bundle<Children>> spawn) { (void)sizeof(spawn); return {}; }
#elif defined(REL_EXCLUSIVE_INSERT)
SystemResult Bad(ExclusiveWorld<> world) { (void)world.Insert(1, Children{}); return {}; }
#elif defined(REL_EXCLUSIVE_REMOVE)
SystemResult Bad(ExclusiveWorld<> world) { (void)world.Remove<Children>(1); return {}; }
#elif defined(REL_EXCLUSIVE_SPAWN)
SystemResult Bad(ExclusiveWorld<> world) { (void)world.Spawn(Children{}); return {}; }
#elif defined(REL_EXCLUSIVE_SPAWN_WITH_ID)
SystemResult Bad(ExclusiveWorld<> world) { (void)world.SpawnWithId(1, Children{}); return {}; }
#elif defined(REL_WORLD_INSERT)
[[maybe_unused]] void Bad(World& world) { world.Insert(1, Children{}); }
#elif defined(REL_WORLD_REMOVE)
[[maybe_unused]] void Bad(World& world) { (void)world.Remove<Children>(1); }
#elif defined(REL_INITIAL_VALUE)
[[maybe_unused]] void Bad(Registrar& registrar) { registrar.InitialValue<Children, &SampleChildren>(); }
#elif defined(REL_DIAGNOSTICS)
SystemResult Bad(DiagnosticsWriter<RejectedRelationship> writer) { (void)sizeof(writer); return {}; }
#endif

} // namespace
