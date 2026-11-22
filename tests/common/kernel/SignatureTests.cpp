// Spec J-10 (trait half), J-11, J-12, J-13: the System contract checked on
// signatures (target/kernel DI-KS 1; spec S-1 to S-5, S-10, B.4).
// J-10's three core rejections are also compile-failure samples built at
// configure time (compile_fail/, migration plan U5).

#include "KernelTestSupport.hpp"

#include <type_traits>

namespace {

using namespace KernelTest;

// ---- Signatures used by J-12. Each is a non-member function; the body is
// empty (Systems do not run in this batch).

SystemResult ReadPos(Query<Read<TestPos>>) { return Ok(); }
SystemResult MovePos(Query<Write<TestPos>, Read<TestVel>>) { return Ok(); }
SystemResult FilterPos(Query<Read<TestPos>, With<TestMarker>, Without<TestMode>>) { return Ok(); }
SystemResult WatchVel(Query<Read<TestPos>, Changed<TestVel>>) { return Ok(); }
SystemResult WatchAdded(Query<Read<TestMode>, Added<TestMarker>>) { return Ok(); }
SystemResult UseResources(Res<TestRes>, ResMut<TestOtherRes>) { return Ok(); }
SystemResult UseEvents(MessageReader<TestEvent>, MessageWriter<TestOtherEvent>) { return Ok(); }
SystemResult UseCommands(Commands, CommandsFor<Inserts<TestHealth>, Removes<TestMarker>>) { return Ok(); }
SystemResult SpawnBodies(Spawn<Bundle<TestPos, TestVel>>) { return Ok(); }
SystemResult SpawnGiven(SpawnWithId<Bundle<TestHealth>>) { return Ok(); }
SystemResult RemoveBodies(Despawn) { return Ok(); }
SystemResult ReadReplicated(SetRead<ReplicatedSet>) { return Ok(); }
SystemResult KeepCount(Local<std::int32_t>, Query<Read<TestMode>>) { return Ok(); }
SystemResult EditWorld(ExclusiveWorld<>) { return Ok(); }
SystemResult TraceVel(DiagnosticsWriter<TestRecord>, Query<Read<TestVel>>) { return Ok(); }
SystemResult ExtractPos(Extract<Read<TestPos>>, ExtractRes<TestRes>, Query<Write<TestMarker>>) { return Ok(); }
SystemResult NoParameters() noexcept { return Ok(); }
SystemResult GatedWrite(Query<Write<TestVel>>) { return Ok(); }
bool ResourceIsSet(Res<TestRes>, Query<With<TestMode>>) { return true; }

// ---- J-10, J-11, J-13 rejected shapes.

struct World {};  // stands for the kernel World type, which this batch does not have
struct Holder {
    SystemResult Member(Query<Read<TestPos>>) { return Ok(); }
};
SystemResult TakesWorld(World&) { return Ok(); }
SystemResult TakesReference(TestPos&) { return Ok(); }
SystemResult TakesPointer(TestPos*) { return Ok(); }
SystemResult TakesInt(int) { return Ok(); }
void ReturnsVoid(Query<Read<TestPos>>) {}
int ReturnsInt(Query<Read<TestPos>>) { return 0; }
SystemResult WritesTwice(Query<Write<TestPos>>, Query<Write<TestPos>>) { return Ok(); }
SystemResult ReadsAndWrites(Query<Read<TestPos>, Write<TestPos>>) { return Ok(); }
SystemResult ReadsAndWritesAcross(Query<Read<TestPos>>, Query<Write<TestPos>>) { return Ok(); }
SystemResult ResBoth(Res<TestRes>, ResMut<TestRes>) { return Ok(); }
SystemResult EventBoth(MessageReader<TestEvent>, MessageWriter<TestEvent>) { return Ok(); }
SystemResult ReadsTwice(Query<Read<TestPos>>, Query<Read<TestPos>, Read<TestVel>>) { return Ok(); }
SystemResult WorldAndMore(ExclusiveWorld<>, Res<TestRes>) { return Ok(); }
using R = Res<TestRes>;
SystemResult Seventeen(R, R, R, R, R, R, R, R, R, R, R, R, R, R, R, R, R) { return Ok(); }
SystemResult Sixteen(R, R, R, R, R, R, R, R, R, R, R, R, R, R, R, R) { return Ok(); }
bool ConditionWrites(Query<Write<TestPos>>) { return true; }
bool ConditionResMut(ResMut<TestRes>) { return true; }
bool ConditionLocal(Local<int>) { return true; }
int ConditionInt(Res<TestRes>) { return 0; }
bool ConditionReads(Res<TestRes>, Query<Read<TestPos>>) { return true; }

template <class P>
using C = typename ParamTraits<P>::Contribution;

} // namespace

TEST_CASE("J-10: a capturing lambda, a generic lambda and a member function are rejected") {
    int captured = 1;
    const auto stateless = [](Query<Read<TestPos>>) -> SystemResult { return {}; };
    const auto byValue = [captured](Query<Read<TestPos>>) -> SystemResult { return captured > 0 ? SystemResult{} : SystemResult{}; };
    const auto byReference = [&captured](Query<Read<TestPos>>) -> SystemResult { return captured > 0 ? SystemResult{} : SystemResult{}; };
    const auto generic = [](auto) -> SystemResult { return {}; };
    struct Overloaded {
        SystemResult operator()(Res<TestRes>) const { return {}; }
        SystemResult operator()(ResMut<TestRes>) const { return {}; }
    };
    struct Functor {  // empty, but a function object with a this, not a captureless lambda
        SystemResult operator()(Res<TestRes>) const { return {}; }
    };
    CHECK(SystemRejectionOf<decltype(stateless)>() == Rejection::None);
    CHECK(SystemRejectionOf<decltype(byValue)>() == Rejection::CapturingCallable);
    CHECK(SystemRejectionOf<decltype(byReference)>() == Rejection::CapturingCallable);
    CHECK(SystemRejectionOf<decltype(generic)>() == Rejection::GenericOrOverloaded);
    CHECK(SystemRejectionOf<Overloaded>() == Rejection::GenericOrOverloaded);
    CHECK(SystemRejectionOf<Functor>() == Rejection::CapturingCallable);
    CHECK(SystemRejectionOf<int>() == Rejection::NotCallable);
    CHECK(SystemRejectionOfFunction<&Holder::Member>() == Rejection::MemberFunction);
    CHECK(SystemRejectionOfFunction<&ReadPos>() == Rejection::None);
    CHECK(SystemRejectionOfFunction<&NoParameters>() == Rejection::None);
}

TEST_CASE("J-10: World&, T&, a pointer and int parameters are rejected") {
    CHECK(SystemRejectionOfFunction<&TakesWorld>() == Rejection::Parameter);
    CHECK(SystemRejectionOfFunction<&TakesReference>() == Rejection::Parameter);
    CHECK(SystemRejectionOfFunction<&TakesPointer>() == Rejection::Parameter);
    CHECK(SystemRejectionOfFunction<&TakesInt>() == Rejection::Parameter);
    CHECK_FALSE(ParamTraits<const TestPos&>::valid);
    CHECK_FALSE(ParamTraits<Query<Read<TestPos>>&>::valid);
}

TEST_CASE("J-10: the return type is Result<void, KernelError>") {
    CHECK(SystemRejectionOfFunction<&ReturnsVoid>() == Rejection::ReturnType);
    CHECK(SystemRejectionOfFunction<&ReturnsInt>() == Rejection::ReturnType);
    const auto lambdaVoid = [](Res<TestRes>) {};
    CHECK(SystemRejectionOf<decltype(lambdaVoid)>() == Rejection::ReturnType);
}

TEST_CASE("J-10: ExclusiveWorld alone, at most 16 parameters; handles cannot be copied or moved") {
    CHECK(SystemRejectionOfFunction<&EditWorld>() == Rejection::None);
    CHECK(SystemRejectionOfFunction<&WorldAndMore>() == Rejection::ExclusiveWorldNotAlone);
    CHECK(SystemRejectionOfFunction<&Sixteen>() == Rejection::None);
    CHECK(SystemRejectionOfFunction<&Seventeen>() == Rejection::TooManyParameters);
    static_assert(!std::is_copy_constructible_v<Query<Read<TestPos>>>);
    static_assert(!std::is_move_constructible_v<Query<Read<TestPos>>>);
    static_assert(!std::is_default_constructible_v<Query<Read<TestPos>>>);
    static_assert(!std::is_copy_constructible_v<ResMut<TestRes>>);
    static_assert(!std::is_default_constructible_v<Commands>);
    static_assert(!std::is_copy_constructible_v<ExclusiveWorld<>>);
    // Extract<Write<T>> has no ParamTraits (DI-KS 13).
    CHECK_FALSE(ParamTraits<Extract<Write<TestPos>>>::valid);
}

TEST_CASE("J-11: one key written twice, or read and written, by one signature is rejected") {
    CHECK(SystemRejectionOfFunction<&WritesTwice>() == Rejection::SelfConflict);
    CHECK(SystemRejectionOfFunction<&ReadsAndWrites>() == Rejection::SelfConflict);
    CHECK(SystemRejectionOfFunction<&ReadsAndWritesAcross>() == Rejection::SelfConflict);
    CHECK(SystemRejectionOfFunction<&ResBoth>() == Rejection::SelfConflict);
    CHECK(SystemRejectionOfFunction<&EventBoth>() == Rejection::SelfConflict);
    // Reads may repeat.
    CHECK(SystemRejectionOfFunction<&ReadsTwice>() == Rejection::None);
}

TEST_CASE("J-13: a run condition has read-only parameters and returns bool") {
    CHECK(ConditionRejectionOfFunction<&ConditionWrites>() == Rejection::ConditionNotReadOnly);
    CHECK(ConditionRejectionOfFunction<&ConditionResMut>() == Rejection::ConditionNotReadOnly);
    CHECK(ConditionRejectionOfFunction<&ConditionLocal>() == Rejection::ConditionNotReadOnly);
    CHECK(ConditionRejectionOfFunction<&ConditionInt>() == Rejection::ConditionReturn);
    CHECK(ConditionRejectionOfFunction<&ConditionReads>() == Rejection::None);
    const auto lambdaWrites = [](ResMut<TestRes>) { return true; };
    CHECK(ConditionRejectionOf<decltype(lambdaWrites)>() == Rejection::ConditionNotReadOnly);
}

TEST_CASE("J-12: every parameter type's Contribution equals the hand-written expectation (types)") {
    // Checked at run time (the values are compile-time constants), so a broken Contribution is a test failure
    // that names the parameter type, not a build failure.
    using namespace GYO::Kernel::Access;
    CHECK(SameSet<C<Query<Read<TestPos>>>, TypeList<ComponentRead<TestPos>>>);
    CHECK(SameSet<C<Query<Write<TestPos>, Read<TestVel>>>,
                          TypeList<ComponentWrite<TestPos>, ComponentRead<TestVel>>>);
    CHECK(SameSet<C<Query<Read<TestPos>, With<TestMarker>, Without<TestMode>>>,
                          TypeList<ComponentRead<TestPos>, Membership<TestMarker>, Membership<TestMode>>>);
    CHECK(SameSet<C<Query<Read<TestPos>, Changed<TestVel>>>,
                          TypeList<ComponentRead<TestPos>, ChangeFilter<TestVel>>>);
    CHECK(SameSet<C<Query<Read<TestMode>, Added<TestMarker>>>,
                          TypeList<ComponentRead<TestMode>, ChangeFilter<TestMarker>>>);
    CHECK(SameSet<C<Res<TestRes>>, TypeList<ResourceRead<TestRes>>>);
    CHECK(SameSet<C<ResMut<TestRes>>, TypeList<ResourceWrite<TestRes>>>);
    CHECK(SameSet<C<MessageReader<TestEvent>>, TypeList<MessageRead<TestEvent>>>);
    CHECK(SameSet<C<MessageWriter<TestEvent>>, TypeList<MessageWrite<TestEvent>>>);
    CHECK(SameSet<C<Commands>, TypeList<UntypedCommands>>);
    CHECK(SameSet<C<CommandsFor<Inserts<TestHealth>, Removes<TestMarker>>>,
                          TypeList<DeferredInsert<TestHealth>, DeferredWrite<TestMarker>>>);
    CHECK(SameSet<C<GYO::Kernel::Spawn<Bundle<TestPos, TestVel>>>,
                          TypeList<Access::Spawn, DeferredInsert<TestPos>, DeferredInsert<TestVel>>>);
    CHECK(SameSet<C<SpawnWithId<Bundle<TestHealth>>>, TypeList<DeferredInsert<TestHealth>>>);
    CHECK(SameSet<C<GYO::Kernel::Despawn>, TypeList<Access::Despawn>>);
    CHECK(SameSet<C<SetRead<ReplicatedSet>>, TypeList<SetReadOf<ReplicatedSet>>>);
    CHECK(SameSet<C<Local<std::int32_t>>, TypeList<LocalState<std::int32_t>>>);
    CHECK(SameSet<C<ExclusiveWorld<>>, TypeList<WholeWorld>>);
    CHECK(SameSet<C<DiagnosticsWriter<TestRecord>>, TypeList<Access::Diagnostics<TestRecord>>>);
    CHECK(SameSet<C<Extract<Read<TestPos>>>, TypeList<ExtractComponentRead<TestPos>>>);
    CHECK(SameSet<C<ExtractRes<TestRes>>, TypeList<ExtractResourceRead<TestRes>>>);
    // A bundle that loses a component is a different set (the J-12 mutation "bundle not in the access set").
    CHECK(!SameSet<C<GYO::Kernel::Spawn<Bundle<TestPos, TestVel>>>, TypeList<Access::Spawn, DeferredInsert<TestPos>>>);
}

// Writes<R...> is an annotation (v8-arch-gyo2-exclusive-writes-optional, DI-KS 1):
// no resource write, not a writer; the listed resources are in annotatedWrites.
TEST_CASE("J-12: ExclusiveWorld<Writes<R>> is no write of R: R is in annotatedWrites only") {
    using namespace GYO::Kernel::Access;
    CHECK_MESSAGE((SameSet<C<ExclusiveWorld<Writes<TestRes>>>, TypeList<WholeWorld, AnnotatedWrite<TestRes>>>),
                  "J-12 annotation: the Contribution of ExclusiveWorld<Writes<TestRes>>");
    Built built = FinishOne([](Registrar& r) {
        const auto lists = [](ExclusiveWorld<Writes<TestOtherRes>>) -> SystemResult { return {}; };
        r.AddSystem<decltype(lists)>(Sys("Lists", {At("Update", "First")}));
    });
    KERNEL_REQUIRE_OK(built);
    const SystemInfo* info = built.registry->Frozen().SystemNamed("Lists");
    REQUIRE(info != nullptr);
    const ResourceId listed = *built.registry->Frozen().ResourceIdOf<TestOtherRes>();
    CHECK(info->access.exclusive);
    CHECK_MESSAGE(info->access.writes.empty(), "J-12 annotation: a listed resource is not a write");
    CHECK_MESSAGE((info->access.annotatedWrites == std::vector<AccessKey>{{AccessKind::Resource, listed}}),
                  "J-12 annotation: annotatedWrites holds the listed resource");
}

// Writes<...> lists components too (v8-arch-gyo2-authority-rollback step 2; authority declaration 5.8): the
// authority Module reads which decision data an exclusive System writes. Still no write and no conflict.
TEST_CASE("J-12: Writes lists a component and a resource in key order and writes neither") {
    Built built = FinishOne([](Registrar& r) {
        const auto lists = [](ExclusiveWorld<Writes<TestOtherRes, TestMarker>>) -> SystemResult { return {}; };
        r.AddSystem<decltype(lists)>(Sys("ListsBoth", {At("Update", "First")}));
    });
    KERNEL_REQUIRE_OK(built);
    const SystemInfo* info = built.registry->Frozen().SystemNamed("ListsBoth");
    REQUIRE(info != nullptr);
    const ComponentId marker = *built.registry->Frozen().ComponentIdOf<TestMarker>();
    const ResourceId other = *built.registry->Frozen().ResourceIdOf<TestOtherRes>();
    CHECK_MESSAGE(info->access.writes.empty(), "[ks-annotated-component] a listed component is not a write");
    CHECK_MESSAGE((info->access.annotatedWrites ==
                   std::vector<AccessKey>{{AccessKind::Component, marker}, {AccessKind::Resource, other}}),
                  "[ks-annotated-component] annotatedWrites holds the listed component and resource");
    Built unknown = FinishOne([](Registrar& r) {
        const auto lists = [](ExclusiveWorld<Writes<TestRecord>>) -> SystemResult { return {}; };
        r.AddSystem<decltype(lists)>(Sys("ListsUnknown", {At("Update", "First")}));
    });
    CHECK_MESSAGE(HasRule(unknown, StartupCode::RE4, "R-6"), "[ks-annotated-unregistered] " << Describe(unknown));
}

namespace {

// The source World of the extract case: its own Registry, frozen first.
Built SourceWorld() {
    return Finish({{"source", [](Registrar& r) {
                        r.Component<TestPos>({.name = "TestPos"});
                        r.Component<TestVel>({.name = "TestVel"});
                        r.Resource<TestRes>({.name = "TestRes"});
                        ScheduleOptions s;
                        s.name = "Main";
                        s.stages = {"Run"};
                        r.Schedule(s);
                    }}});
}

void RegisterJ12Data(Registrar& r) {
    // Every type zone B so no zone rule fires; ids follow this order.
    r.Component<TestPos>({.name = "TestPos"});      // 0
    r.Component<TestVel>({.name = "TestVel"});      // 1
    r.Component<TestMarker>({.name = "TestMarker"});  // 2
    r.Component<TestMode>({.name = "TestMode"});    // 3
    r.Component<TestHealth>({.name = "TestHealth"});  // 4
    r.Resource<TestRes>({.name = "TestRes"});       // 1 (0 is KernelSettings)
    r.Resource<TestOtherRes>({.name = "TestOtherRes"});  // 2
    r.Resource<TestAllotter>({.name = "TestAllotter"});  // 3
    r.Allotter<TestAllotter>();
    r.Message<TestEvent>({.name = "TestEvent"});       // 0
    r.Message<TestOtherEvent>({.name = "TestOtherEvent"});  // 1
    r.NamedSet<ReplicatedSet>("Replicated");
    r.AddToSet<ReplicatedSet, TestPos>();
    r.AddToSet<ReplicatedSet, TestHealth>();
    ScheduleOptions update = UpdateSchedule();
    update.stages = {"S0", "S1", "S2", "S3", "S4", "S5", "S6", "S7", "S8", "S9", "S10", "S11", "S12", "S13", "S14",
                     "S15", "S16", "S17", "S18", "S19"};
    r.Schedule(update);
    ScheduleOptions extract;
    extract.name = "Extract";
    extract.extract = true;
    extract.stages = {"Copy"};
    r.Schedule(extract);
}

AccessKey Comp(std::uint32_t id) { return {AccessKind::Component, id}; }
AccessKey Resrc(std::uint32_t id) { return {AccessKind::Resource, id}; }
AccessKey Evt(std::uint32_t id) { return {AccessKind::Message, id}; }

} // namespace

TEST_CASE("J-12: every signature's access set in ids equals the hand-written expectation") {
    Built source = SourceWorld();
    KERNEL_REQUIRE_OK(source);
    const auto build = [](Registrar& r) {
        RegisterJ12Data(r);
        int stage = 0;
        const auto at = [&stage]() { return std::vector<Mount>{At("Update", "S" + std::to_string(stage++))}; };
        r.AddSystem<&ReadPos>(Sys("ReadPos", at()));
        r.AddSystem<&MovePos>(Sys("MovePos", at()));
        r.AddSystem<&FilterPos>(Sys("FilterPos", at()));
        r.AddSystem<&WatchVel>(Sys("WatchVel", at()));
        r.AddSystem<&WatchAdded>(Sys("WatchAdded", at()));
        r.AddSystem<&UseResources>(Sys("UseResources", at()));
        r.AddSystem<&UseEvents>(Sys("UseEvents", at()));
        r.AddSystem<&UseCommands>(Sys("UseCommands", at()));
        r.AddSystem<&SpawnBodies>(Sys("SpawnBodies", at()));
        r.AddSystem<&SpawnGiven>(Sys("SpawnGiven", at()));
        r.AddSystem<&RemoveBodies>(Sys("RemoveBodies", at()));
        r.AddSystem<&ReadReplicated>(Sys("ReadReplicated", at()));
        r.AddSystem<&KeepCount>(Sys("KeepCount", at()));
        r.AddSystem<&EditWorld>(Sys("EditWorld", at()));
        r.AddSystem<&TraceVel>(Sys("TraceVel", at()));
        r.AddSystem<&ExtractPos>(Sys("ExtractPos", {At("Extract", "Copy")}));
        r.AddSystem<&NoParameters>(Sys("NoParameters", at()));
        Mount gated = At("Update", "S" + std::to_string(stage++));
        gated.conditions.push_back(Condition<&ResourceIsSet>("ResourceIsSet"));
        r.AddSystem<&GatedWrite>(Sys("GatedWrite", {gated}));
        const auto lambda = [](Query<Read<TestVel>>, ResMut<TestRes>) -> SystemResult { return {}; };
        r.AddSystem<decltype(lambda)>(Sys("LambdaSystem", at()));
    };
    Registry registry;
    registry.SetExtractSource(*source.registry);
    const auto result = RegisterAll(registry, {{"j12", build}});
    REQUIRE_MESSAGE(static_cast<bool>(result), (result ? std::string() : result.error().detail));
    const FrozenRegistry& frozen = registry.Frozen();
    const auto access = [&](const char* name) -> const AccessSet& {
        const SystemInfo* info = frozen.SystemNamed(name);
        REQUIRE(info != nullptr);
        return info->access;
    };
    using Keys = std::vector<AccessKey>;
    using Ids = std::vector<ComponentId>;
    CHECK(access("ReadPos").reads == Keys{Comp(0)});
    CHECK(access("ReadPos").writes.empty());
    CHECK(access("MovePos").reads == Keys{Comp(1)});
    CHECK(access("MovePos").writes == Keys{Comp(0)});
    CHECK(access("FilterPos").reads == Keys{Comp(0)});
    CHECK(access("FilterPos").membershipReads == Ids{2, 3});
    CHECK_FALSE(access("FilterPos").changeFilters);
    CHECK(access("WatchVel").membershipReads == Ids{1});
    CHECK(access("WatchVel").changeFilters);
    CHECK(access("WatchAdded").reads == Keys{Comp(3)});
    CHECK(access("WatchAdded").membershipReads == Ids{2});
    CHECK(access("WatchAdded").changeFilters);
    CHECK(access("UseResources").reads == Keys{Resrc(1)});
    CHECK(access("UseResources").writes == Keys{Resrc(2)});
    CHECK(access("UseEvents").reads == Keys{Evt(0)});
    CHECK(access("UseEvents").writes == Keys{Evt(1)});
    CHECK(access("UseCommands").untypedCommands);
    CHECK(access("UseCommands").deferredWrites == Ids{2, 4});
    CHECK(access("UseCommands").writes.empty());
    CHECK(access("SpawnBodies").spawn);
    CHECK(access("SpawnBodies").writes == Keys{Resrc(3)});
    CHECK(access("SpawnBodies").deferredWrites == Ids{0, 1});
    CHECK_FALSE(access("SpawnGiven").spawn);
    CHECK(access("SpawnGiven").writes.empty());
    CHECK(access("SpawnGiven").deferredWrites == Ids{4});
    CHECK(access("RemoveBodies").despawn);
    CHECK(access("RemoveBodies").writes.empty());
    CHECK(access("RemoveBodies").deferredWrites.empty());
    CHECK(access("ReadReplicated").setReads == std::vector<SetId>{0});
    CHECK(access("ReadReplicated").reads == Keys{Comp(0), Comp(4)});
    CHECK(access("KeepCount").local);
    CHECK(access("KeepCount").reads == Keys{Comp(3)});
    CHECK(access("EditWorld").exclusive);
    CHECK(access("EditWorld").reads.empty());
    CHECK(access("TraceVel").diagnostics);
    CHECK(access("TraceVel").reads == Keys{Comp(1)});
    CHECK(access("ExtractPos").extractReads == Keys{Comp(0), Resrc(1)});
    CHECK(access("ExtractPos").writes == Keys{Comp(2)});
    CHECK(access("ExtractPos").reads.empty());
    CHECK(access("NoParameters").reads.empty());
    CHECK(access("NoParameters").writes.empty());
    // Run condition reads join the System's reads (spec S-4).
    CHECK(access("GatedWrite").writes == Keys{Comp(1)});
    CHECK(access("GatedWrite").reads == Keys{Resrc(1)});
    CHECK(access("GatedWrite").membershipReads == Ids{3});
    CHECK(access("LambdaSystem").reads == Keys{Comp(1)});
    CHECK(access("LambdaSystem").writes == Keys{Resrc(1)});
    // Flags that none of the others set stay off.
    CHECK_FALSE(access("ReadPos").local);
    CHECK_FALSE(access("ReadPos").exclusive);
    CHECK_FALSE(access("ReadPos").diagnostics);
    CHECK_FALSE(access("ReadPos").spawn);
    CHECK_FALSE(access("ReadPos").despawn);
    CHECK_FALSE(access("ReadPos").untypedCommands);
}
