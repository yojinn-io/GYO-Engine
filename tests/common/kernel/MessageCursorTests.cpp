// Retained messages read by cursor (v8-arch-gyo2-time-events step 3; event
// cursor declaration 3.2, 5.1 and section 8 "new: retained"; DI-KS 10 draft):
// one cursor per reader (a System's mount, or one of its run conditions) per
// message type, kept in the World; a reader reads every record after its
// cursor, in write order, and its cursor moves to the last one it read; a
// reader that does not run keeps its cursor and reads what it missed later
// (within the capacity); a reader holds records back only once it has run; a
// write to a full buffer drops the oldest record and the World records one
// DroppedMessages per message type per outermost Run; records read by every
// reader that holds them are trimmed when the outermost Run ends, after the
// hook rounds (not after a nested run, not after an Err).
// Each CHECK carries a tag in brackets ("[msg-...]"): the mutants of the step
// expect that tag (docs/architecture/plans/kernel/scripts/mutations.json, ids msg-).

#include "WorldTestSupport.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace {

using namespace WorldTest;

// A message with a capacity of 3 (the cap and the dropped-records record).
struct Beep { std::uint32_t n; };

std::string Joined() {
    std::string out;
    for (const std::string& line : Log()) out += (out.empty() ? "" : " | ") + line;
    return out;
}

std::string Numbers(const std::vector<const Ping*>& pings) {
    std::string out;
    for (const Ping* ping : pings) out += (out.empty() ? "" : " ") + std::to_string(ping->n);
    return out;
}

std::string Numbers(const std::vector<Ping>& pings) {
    std::string out;
    for (const Ping& ping : pings) out += (out.empty() ? "" : " ") + std::to_string(ping.n);
    return out;
}

// ---- Systems (captureless; what they see goes to Log())

// Writes Ping{run number} on every run (its own count, one per mount: B-12).
SystemResult WriteEveryRun(MessageWriter<Ping> pings, Local<std::uint32_t> runs) {
    pings.Send(Ping{++*runs});
    return Ok();
}
// Writes Ping{run number} on its first and third run only.
SystemResult WriteFirstAndThird(MessageWriter<Ping> pings, Local<std::uint32_t> runs) {
    ++*runs;
    if (*runs == 1 || *runs == 3) pings.Send(Ping{*runs});
    return Ok();
}
// Writes one Ping on its first run only.
SystemResult WriteOnce(MessageWriter<Ping> pings, Local<std::uint32_t> runs) {
    if (++*runs == 1) pings.Send(Ping{1});
    return Ok();
}
// Writes one Ping on each of its first two runs.
SystemResult WriteTwice(MessageWriter<Ping> pings, Local<std::uint32_t> runs) {
    if (++*runs <= 2) pings.Send(Ping{*runs});
    return Ok();
}
SystemResult Reader(MessageReader<Ping> pings) {
    Log().push_back("read:" + Numbers(pings.Read()));
    return Ok();
}
SystemResult LaterReader(MessageReader<Ping> pings) {
    Log().push_back("later:" + Numbers(pings.Read()));
    return Ok();
}
// Reads twice in one call: the same records both times.
SystemResult ReadsTwice(MessageReader<Ping> pings) {
    const std::vector<const Ping*> first = pings.Read();
    const std::vector<const Ping*> second = pings.Read();
    Log().push_back("first:" + Numbers(first) + " second:" + Numbers(second));
    return Ok();
}
// A run condition with its own cursor.
bool HasPings(MessageReader<Ping> pings) {
    const std::vector<const Ping*> read = pings.Read();
    Log().push_back("cond:" + Numbers(read));
    return !read.empty();
}
bool FlagOn(Res<Flag> flag) { return flag->on != 0; }
bool OutSet(Res<Out> out) { return out->value != 0; }
// Writes five Beeps on its second run (the buffer keeps 3).
SystemResult WriteFiveBeepsOnSecondRun(MessageWriter<Beep> beeps, Local<std::uint32_t> runs) {
    if (++*runs != 2) return Ok();
    for (std::uint32_t n = 1; n <= 5; ++n) beeps.Send(Beep{n});
    return Ok();
}
// Writes Beep{10 * run + 1..3} on every run (run from 0).
SystemResult WriteThreeBeeps(MessageWriter<Beep> beeps, Local<std::uint32_t> runs) {
    for (std::uint32_t n = 1; n <= 3; ++n) beeps.Send(Beep{*runs * 10 + n});
    ++*runs;
    return Ok();
}
// Writes Beep{10 * run + 9} on every run (run from 0).
SystemResult WriteOneBeep(MessageWriter<Beep> beeps, Local<std::uint32_t> runs) {
    beeps.Send(Beep{*runs * 10 + 9});
    ++*runs;
    return Ok();
}
SystemResult BeepReader(MessageReader<Beep> beeps) {
    std::string line = "beeps:";
    for (const Beep* beep : beeps.Read()) line += " " + std::to_string(beep->n);
    Log().push_back(line);
    return Ok();
}
// Spawns a new Tag entity on every run (its OnAdd hook writes a Ping at the sync point).
SystemResult SpawnTagEachRun(SpawnWithId<Bundle<Tag>> spawn, Local<std::uint32_t> runs) {
    spawn.New(100 + ++*runs, Tag{1});
    return Ok();
}
// Two MessageReader<Ping> in one call: one reader.
SystemResult ReadsWithTwoReaders(MessageReader<Ping> first, MessageReader<Ping> second) {
    Log().push_back("two:" + Numbers(first.Read()) + "/" + Numbers(second.Read()));
    return Ok();
}
SystemResult PingOnTag(HookCtx ctx, MessageWriter<Ping> pings) {
    pings.Send(Ping{static_cast<std::uint32_t>(ctx.Entity())});
    return Ok();
}
// OnAdd<Tag> inserts Mode (applied by the next hook round); OnAdd<Mode> writes a Ping.
SystemResult TagAddsMode(HookCtx ctx, CommandsFor<Inserts<Mode>, Removes<>> commands) {
    commands.Insert(ctx.Entity(), Mode{1});
    return Ok();
}
SystemResult PingOnMode(HookCtx, MessageWriter<Ping> pings) {
    pings.Send(Ping{77});
    return Ok();
}

ScheduleOptions SpawnerSchedule() {
    ScheduleOptions s;
    s.name = "Spawner";
    s.stages = {"S"};
    return s;
}

const MessageInfo* InfoNamed(const FrozenRegistry& frozen, const std::string& name) {
    for (const MessageInfo& info : frozen.Messages()) {
        if (info.name == name) return &info;
    }
    return nullptr;
}

std::string ReaderText(const MessageReaderInfo& reader) {
    return reader.system + "@" + reader.schedule + (reader.condition.empty() ? "" : "/" + reader.condition);
}

std::string ReadersText(const std::vector<MessageReaderInfo>& readers) {
    std::string out;
    for (const MessageReaderInfo& reader : readers) out += (out.empty() ? "" : ", ") + ReaderText(reader);
    return out;
}

} // namespace

TEST_CASE("J-36: a reader skipped by its run condition keeps its cursor and reads what it missed when it runs again") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&WriteEveryRun>(B("Write", "A"));
        SystemOptions reader = B("Reader", "B");
        reader.mounts[0].conditions = {Condition<&FlagOn>("FlagOn")};
        r.AddSystem<&Reader>(std::move(reader));
    });
    Log().clear();
    rig->world().SetResource(Flag{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    rig->world().SetResource(Flag{0});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    // The reader has read 1; 2 and 3 are kept for it.
    CHECK_MESSAGE(Numbers(rig->world().MessagePayloads<Ping>()) == "2 3",
                  "[msg-skipped-keeps] " << Numbers(rig->world().MessagePayloads<Ping>()));
    rig->world().SetResource(Flag{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(Joined() == "read:1 | read:2 3 4", "[msg-skipped-catches-up] " << Joined());
    // Everything is read by the one reader that holds them: trimmed when the Run ended.
    CHECK_MESSAGE(rig->world().MessagePayloads<Ping>().empty(), "[msg-trim-read] " << Numbers(rig->world().MessagePayloads<Ping>()));
}

TEST_CASE("J-36: a reader whose schedule did not run reads what was written meanwhile when its schedule runs") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.Schedule(SpawnerSchedule());
        r.AddSystem<&SpawnTagEachRun>(Sys("SpawnTagEachRun", Zone::B, {At("Spawner", "S")}));
        r.Hook<Tag, &PingOnTag>(HookTrigger::OnAdd, {.name = "PingOnTag", .zone = Zone::B});
        r.AddSystem<&Reader>(B("Reader", "A"));
    });
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(rig->Run("Tick"));     // the reader runs once: it holds what comes after
    KERNEL_REQUIRE_RUN_OK(rig->Run("Spawner"));  // the hook writes 101
    KERNEL_REQUIRE_RUN_OK(rig->Run("Spawner"));  // and 102
    CHECK_MESSAGE(Numbers(rig->world().MessagePayloads<Ping>()) == "101 102",
                  "[msg-not-run-keeps] " << Numbers(rig->world().MessagePayloads<Ping>()));
    KERNEL_REQUIRE_RUN_OK(rig->Run("Tick"));
    CHECK_MESSAGE(Joined() == "read: | read:101 102", "[msg-not-run-catches-up] " << Joined());
}

TEST_CASE("J-36: a reader that has never run holds nothing back; when it first runs it reads from the oldest record kept") {
    SUBCASE("no reader has run: the records are trimmed when the Run ends") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.Schedule(SpawnerSchedule());
            r.AddSystem<&SpawnTagEachRun>(Sys("SpawnTagEachRun", Zone::B, {At("Spawner", "S")}));
            r.Hook<Tag, &PingOnTag>(HookTrigger::OnAdd, {.name = "PingOnTag", .zone = Zone::B});
            r.AddSystem<&Reader>(B("Reader", "A"));
        });
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(rig->Run("Spawner"));
        CHECK_MESSAGE(rig->world().MessagePayloads<Ping>().empty(),
                      "[msg-never-run-holds-nothing] " << Numbers(rig->world().MessagePayloads<Ping>()));
        KERNEL_REQUIRE_RUN_OK(rig->Run("Tick"));
        CHECK_MESSAGE(Joined() == "read:", "[msg-never-run-holds-nothing] " << Joined());
    }
    SUBCASE("another reader holds them: the new reader's first run reads from the oldest kept") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&WriteEveryRun>(B("Write", "A"));
            SystemOptions holder = B("Reader", "B");
            holder.mounts[0].conditions = {Condition<&FlagOn>("FlagOn")};
            r.AddSystem<&Reader>(std::move(holder));
            SystemOptions later = B("LaterReader", "C");
            later.mounts[0].conditions = {Condition<&OutSet>("OutSet")};
            r.AddSystem<&LaterReader>(std::move(later));
        });
        Log().clear();
        rig->world().SetResource(Out{0});
        rig->world().SetResource(Flag{1});
        KERNEL_REQUIRE_RUN_OK(rig->Run());  // read:1; the holder holds what comes after
        rig->world().SetResource(Flag{0});
        KERNEL_REQUIRE_RUN_OK(rig->Run());  // 2
        KERNEL_REQUIRE_RUN_OK(rig->Run());  // 3
        rig->world().SetResource(Out{1});
        KERNEL_REQUIRE_RUN_OK(rig->Run());  // 4; the later reader's first run reads 2 3 4
        CHECK_MESSAGE(Joined() == "read:1 | later:2 3 4", "[msg-new-reader-oldest] " << Joined());
    }
    SUBCASE("a reader added by a later round (L5) reads from the oldest kept") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&WriteEveryRun>(B("Write", "A"));
            SystemOptions holder = B("Reader", "B");
            holder.mounts[0].conditions = {Condition<&FlagOn>("FlagOn")};
            r.AddSystem<&Reader>(std::move(holder));
        });
        Log().clear();
        rig->world().SetResource(Flag{1});
        KERNEL_REQUIRE_RUN_OK(rig->Run());  // read:1; the holder holds what comes after
        rig->world().SetResource(Flag{0});
        KERNEL_REQUIRE_RUN_OK(rig->Run());  // 2
        KERNEL_REQUIRE_RUN_OK(rig->Run());  // 3
        REQUIRE(static_cast<bool>(rig->built.registry->Register(RegistrationOwner{"later", 9}, [](Registrar& r) {
            r.AddSystem<&LaterReader>(B("LaterReader", "C"));
        })));
        REQUIRE(static_cast<bool>(rig->built.registry->BuildPlans()));
        KERNEL_REQUIRE_RUN_OK(rig->Run());  // 4; the new reader's first run reads 2 3 4
        CHECK_MESSAGE(Joined() == "read:1 | later:2 3 4", "[msg-new-reader-oldest] " << Joined());
    }
}

TEST_CASE("J-36: removing a reader (L5) drops its cursor, so what only it held is trimmed") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&WriteEveryRun>(B("Write", "A"));
        SystemOptions holder = B("Reader", "B");
        holder.mounts[0].conditions = {Condition<&FlagOn>("FlagOn")};
        r.AddSystem<&Reader>(std::move(holder));
    });
    rig->world().SetResource(Flag{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    rig->world().SetResource(Flag{0});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE(Numbers(rig->world().MessagePayloads<Ping>()) == "2 3");
    REQUIRE(static_cast<bool>(rig->built.registry->Register(RegistrationOwner{"later", 9}, [](Registrar& r) {
        CHECK(r.RemoveSystem("Reader"));
    })));
    REQUIRE(static_cast<bool>(rig->built.registry->BuildPlans()));
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(rig->world().MessagePayloads<Ping>().empty(),
                  "[msg-removed-reader-releases] " << Numbers(rig->world().MessagePayloads<Ping>()));
}

TEST_CASE("J-36: a run condition is a reader of its own: its cursor is not the System's") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&WriteFirstAndThird>(B("Write", "A"));
        SystemOptions reader = B("Reader", "B");
        reader.mounts[0].conditions = {Condition<&HasPings>("HasPings")};
        r.AddSystem<&Reader>(std::move(reader));
    });
    Log().clear();
    for (int run = 0; run < 3; ++run) KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(Joined() == "cond:1 | read:1 | cond: | cond:3 | read:3", "[msg-condition-cursor] " << Joined());
}

TEST_CASE("J-36: Read twice in one call gives the same records; the cursor moves when the call has read them") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&WriteEveryRun>(B("Write", "A"));
        r.AddSystem<&ReadsTwice>(B("ReadsTwice", "B"));
    });
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(Joined() == "first:1 second:1 | first:2 second:2", "[msg-same-batch] " << Joined());
}

TEST_CASE("J-36: a write to a full buffer drops the oldest record; one DroppedMessages per message type per Run") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.Message<Beep>({.name = "Beep", .zone = Zone::B, .capacity = 3});
        r.AddSystem<&BeepReader>(B("BeepReader", "A"));
        r.AddSystem<&WriteFiveBeepsOnSecondRun>(B("WriteBeeps", "B"));
    });
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // the reader runs once (nothing yet): it holds what comes after
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // five Beeps into a buffer of 3: 1 and 2 dropped
    std::vector<std::uint32_t> kept;
    for (const Beep& beep : rig->world().MessagePayloads<Beep>()) kept.push_back(beep.n);
    CHECK_MESSAGE(kept == (std::vector<std::uint32_t>{3, 4, 5}), "[msg-cap-drops-oldest] kept " << kept.size());
    const std::vector<DroppedMessages> dropped = rig->world().TakeDiagnostics<DroppedMessages>();
    REQUIRE_MESSAGE(dropped.size() == 1, "[msg-dropped-one-per-run] " << dropped.size() << " records");
    CHECK_MESSAGE(dropped[0].message == "Beep", "[msg-dropped-fields] " << dropped[0].message);
    CHECK_MESSAGE(dropped[0].count == 2, "[msg-dropped-count] " << dropped[0].count);
    CHECK_MESSAGE((dropped[0].firstSeq == 1 && dropped[0].lastSeq == 2),
                  "[msg-dropped-range] " << dropped[0].firstSeq << ".." << dropped[0].lastSeq);
    CHECK_MESSAGE(ReadersText(dropped[0].readers) == "BeepReader@Tick", "[msg-dropped-readers] " << ReadersText(dropped[0].readers));
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // reads 3 4 5; nothing dropped
    CHECK_MESSAGE(Joined() == "beeps: | beeps: | beeps: 3 4 5", "[msg-cap-drops-oldest] " << Joined());
    CHECK_MESSAGE(rig->world().TakeDiagnostics<DroppedMessages>().empty(), "[msg-dropped-one-per-run] a Run with no drop recorded one");
}

TEST_CASE("J-36: drops in one Run make one record; a reader that has read a dropped record is not listed") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.Message<Beep>({.name = "Beep", .zone = Zone::B, .capacity = 3});
        r.AddSystem<&WriteThreeBeeps>(B("WriteThree", "A"));
        r.AddSystem<&BeepReader>(B("BeepReader", "B"));
        r.AddSystem<&WriteOneBeep>(B("WriteOne", "C"));
    });
    Log().clear();
    // Run 1: 1 2 3 (seq 1..3) read; 9 (seq 4) drops seq 1, which the reader has read.
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    // Run 2: 11 12 13 (seq 5..7) drop seq 4 (9, unread); 19 (seq 8) drops seq 5 (11, read).
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    const std::vector<DroppedMessages> dropped = rig->world().TakeDiagnostics<DroppedMessages>();
    REQUIRE_MESSAGE(dropped.size() == 2, "[msg-dropped-one-per-run] " << dropped.size() << " records");
    CHECK_MESSAGE((dropped[0].count == 1 && dropped[0].firstSeq == 1 && dropped[0].lastSeq == 1),
                  "[msg-dropped-fields] Run 1: " << dropped[0].count << " " << dropped[0].firstSeq << ".." << dropped[0].lastSeq);
    CHECK_MESSAGE(dropped[0].readers.empty(), "[msg-dropped-readers] Run 1: " << ReadersText(dropped[0].readers));
    CHECK_MESSAGE((dropped[1].count == 2 && dropped[1].firstSeq == 4 && dropped[1].lastSeq == 5),
                  "[msg-dropped-fields] Run 2: " << dropped[1].count << " " << dropped[1].firstSeq << ".." << dropped[1].lastSeq);
    CHECK_MESSAGE(ReadersText(dropped[1].readers) == "BeepReader@Tick", "[msg-dropped-readers] Run 2: " << ReadersText(dropped[1].readers));
    CHECK_MESSAGE(Joined() == "beeps: 1 2 3 | beeps: 11 12 13", "[msg-cap-drops-oldest] " << Joined());
}

TEST_CASE("J-36: the Registry lists each message type's capacity and its readers (DI-KS 25)") {
    const Built built = Finish({{"test", [](Registrar& r) {
                                     Data(r);
                                     r.Message<Beep>({.name = "Beep", .zone = Zone::B, .capacity = 3});
                                     r.AddSystem<&WriteEveryRun>(B("Write", "A"));
                                     SystemOptions reader = B("Reader", "B");
                                     reader.mounts[0].conditions = {Condition<&HasPings>("HasPings")};
                                     r.AddSystem<&Reader>(std::move(reader));
                                     r.AddSystem<&ReadsTwice>(B("ReadsTwice", "C"));
                                 }}});
    KERNEL_REQUIRE_OK(built);
    const MessageInfo* ping = InfoNamed(built.registry->Frozen(), "Ping");
    const MessageInfo* beep = InfoNamed(built.registry->Frozen(), "Beep");
    REQUIRE(ping != nullptr);
    REQUIRE(beep != nullptr);
    CHECK_MESSAGE(ping->capacity == 1024, "[msg-registry-capacity] " << ping->capacity);
    CHECK_MESSAGE(beep->capacity == 3, "[msg-registry-capacity] " << beep->capacity);
    CHECK_MESSAGE(ReadersText(ping->readers) == "Reader@Tick, Reader@Tick/HasPings, ReadsTwice@Tick",
                  "[msg-registry-readers] " << ReadersText(ping->readers));
    CHECK_MESSAGE(beep->readers.empty(), "[msg-registry-readers] " << ReadersText(beep->readers));
}

TEST_CASE("J-36: the records are trimmed when the outermost Run ends, after the hook rounds") {
    // Each Run spawns a Tag; its OnAdd hook inserts Mode, applied by hook round 2,
    // whose OnAdd hook writes Ping 77: after the last sync point.
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&SpawnTagEachRun>(B("SpawnTagEachRun", "C"));
        r.Hook<Tag, &TagAddsMode>(HookTrigger::OnAdd, {.name = "TagAddsMode", .zone = Zone::B});
        r.Hook<Mode, &PingOnMode>(HookTrigger::OnAdd, {.name = "PingOnMode", .zone = Zone::B});
        SystemOptions reader = B("Reader", "A");
        reader.mounts[0].conditions = {Condition<&FlagOn>("FlagOn")};
        r.AddSystem<&Reader>(std::move(reader));
    });
    Log().clear();
    SUBCASE("no reader has run: a trim after the rounds drops the round's record, one before them would not") {
        rig->world().SetResource(Flag{0});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(rig->world().MessagePayloads<Ping>().empty(),
                      "[msg-trim-after-rounds] " << Numbers(rig->world().MessagePayloads<Ping>()));
    }
    SUBCASE("the reader has run: it holds the round's record and reads it in the next Run") {
        rig->world().SetResource(Flag{1});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(Numbers(rig->world().MessagePayloads<Ping>()) == "77",
                      "[msg-round-record-held] " << Numbers(rig->world().MessagePayloads<Ping>()));
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(Joined() == "read: | read:77", "[msg-round-record-held] " << Joined());
    }
}

TEST_CASE("J-36: with verification on, reading and the trim at the end of a Run are not changes outside the World API") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&WriteEveryRun>(B("Write", "A"));
        r.AddSystem<&Reader>(B("Reader", "B"));
    }, true);
    for (int run = 0; run < 3; ++run) {
        CAPTURE(run);
        const auto result = rig->Run();
        CHECK_MESSAGE(static_cast<bool>(result), "[msg-trim-verification-quiet] " << Text(result));
    }
    CHECK(rig->world().MessagePayloads<Ping>().empty());
}

TEST_CASE("J-36: the World summary holds the message cursors and the write sequence (event cursor declaration 3.2)") {
    auto summary = [](std::function<void(Registrar&)> build, int runs) {
        auto rig = MakeRig(std::move(build), false);
        for (int run = 0; run < runs; ++run) KERNEL_REQUIRE_RUN_OK(rig->Run());
        return SummaryOf(rig->world());
    };
    const auto writerOnly = [](Registrar& r) {
        Data(r);
        r.AddSystem<&WriteOnce>(B("Write", "A"));
    };
    const auto writerAndReader = [](Registrar& r) {
        Data(r);
        r.AddSystem<&WriteOnce>(B("Write", "A"));
        r.AddSystem<&Reader>(B("Reader", "B"));
    };
    const auto writesTwice = [](Registrar& r) {
        Data(r);
        r.AddSystem<&WriteTwice>(B("Write", "A"));
    };
    // Equal setups, equal summaries (so the differences below are the cursors' and the sequence's).
    CHECK(summary(writerOnly, 2) == summary(writerOnly, 2));
    // Both trimmed to nothing; one World has a reader's cursor.
    CHECK_MESSAGE(summary(writerOnly, 2) != summary(writerAndReader, 2), "[msg-summary-cursors] the cursor table is not in the summary");
    // Both trimmed to nothing; one World wrote one more record.
    CHECK_MESSAGE(summary(writerOnly, 2) != summary(writesTwice, 2), "[msg-summary-seq] the write sequence is not in the summary");
}

TEST_CASE("J-18: a message type's capacity changes the Registry fingerprint and, for a zone C type, the C fingerprint") {
    const auto build = [](Zone zone, std::uint32_t capacity) {
        return Finish({{"test", [zone, capacity](Registrar& r) {
                            Data(r);
                            r.Message<Beep>({.name = "Beep", .zone = zone, .capacity = capacity});
                        }}});
    };
    const Built b3 = build(Zone::B, 3);
    const Built b4 = build(Zone::B, 4);
    const Built c3 = build(Zone::C, 3);
    const Built c4 = build(Zone::C, 4);
    KERNEL_REQUIRE_OK(b3);
    KERNEL_REQUIRE_OK(b4);
    KERNEL_REQUIRE_OK(c3);
    KERNEL_REQUIRE_OK(c4);
    CHECK_MESSAGE(b3.registry->Frozen().RegistryFingerprint() != b4.registry->Frozen().RegistryFingerprint(),
                  "[msg-capacity-registry-fp] the capacity is not in the Registry fingerprint");
    // A zone B type is not in the C fingerprint at all.
    CHECK(b3.registry->Frozen().CFingerprint() == b4.registry->Frozen().CFingerprint());
    CHECK_MESSAGE(c3.registry->Frozen().CFingerprint() != c4.registry->Frozen().CFingerprint(),
                  "[msg-capacity-c-fp] a zone C type's capacity is not in the C fingerprint");
}

// ---- Step 7: the cases the adversarial check of the mutant list found missing

TEST_CASE("J-36: the Registry's readers: a named set's run condition; two MessageReader of one call are one; a later BuildPlans lists them anew") {
    Built built = Finish({{"test", [](Registrar& r) {
                               Data(r);
                               r.Set({.name = "Gated", .schedule = "Tick"}, {Condition<&HasPings>("HasPings")});
                               SystemOptions member = B("Member", "B");
                               member.mounts[0].sets = {"Gated"};
                               r.AddSystem<&Reader>(std::move(member));
                               r.AddSystem<&ReadsWithTwoReaders>(B("Twice", "C"));
                           }}});
    KERNEL_REQUIRE_OK(built);
    const MessageInfo* ping = InfoNamed(built.registry->Frozen(), "Ping");
    REQUIRE(ping != nullptr);
    CHECK_MESSAGE(ReadersText(ping->readers).find("Member@Tick/HasPings") != std::string::npos,
                  "[msg-registry-set-condition] " << ReadersText(ping->readers));
    std::size_t twice = 0;
    for (const MessageReaderInfo& reader : ping->readers) twice += reader.system == "Twice" ? 1U : 0U;
    CHECK_MESSAGE(twice == 1, "[msg-registry-one-reader-per-call] " << ReadersText(ping->readers));
    REQUIRE(static_cast<bool>(built.registry->Register(RegistrationOwner{"later", 9}, [](Registrar& r) {
        CHECK(r.RemoveSystem("Twice"));
    })));
    REQUIRE(static_cast<bool>(built.registry->BuildPlans()));
    const MessageInfo* after = InfoNamed(built.registry->Frozen(), "Ping");
    REQUIRE(after != nullptr);
    CHECK_MESSAGE(ReadersText(after->readers) == "Member@Tick, Member@Tick/HasPings", "[msg-registry-readers-rebuilt] " << ReadersText(after->readers));
}

TEST_CASE("J-36: a message type's capacity is at least 1: 0 is a Programmer Error at registration") {
    GYO_CHECK_ASSERTS(Finish({{"test", [](Registrar& r) {
        Data(r);
        r.Message<Beep>({.name = "Beep", .zone = Zone::B, .capacity = 0});
    }}}));
}

TEST_CASE("J-36: the World summary holds where each cursor has read to") {
    // Two Worlds with the same records kept (Slow holds them) and the same resources; only Fast's cursor differs.
    auto summary = [](bool fastRunsOn) {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&WriteEveryRun>(B("Write", "A"));
            SystemOptions slow = B("Slow", "B");
            slow.mounts[0].conditions = {Condition<&FlagOn>("FlagOn")};
            r.AddSystem<&Reader>(std::move(slow));
            SystemOptions fast = B("Fast", "C");
            fast.mounts[0].conditions = {Condition<&OutSet>("OutSet")};
            r.AddSystem<&LaterReader>(std::move(fast));
        }, false);
        rig->world().SetResource(Flag{1});
        rig->world().SetResource(Out{1});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        rig->world().SetResource(Flag{0});
        rig->world().SetResource(Out{fastRunsOn ? 1 : 0});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        rig->world().SetResource(Out{1});
        REQUIRE(Numbers(rig->world().MessagePayloads<Ping>()) == "2 3");
        return SummaryOf(rig->world());
    };
    CHECK(summary(true) == summary(true));
    CHECK_MESSAGE(summary(true) != summary(false), "[msg-summary-cursor-position] where a cursor has read to is not in the summary");
}
