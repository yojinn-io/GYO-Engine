// Extract (declaration docs/architecture/target/v2/sdl-extract-declaration.zh-Hant.md,
// v8-arch-gyo2-sdl-extract, section 2.6): WorldExecutor::RunExtract reads a source World
// through const access only; the Extract parameters' read methods; D-E7 for run
// conditions; the asserts of RunExtract and Run. target/kernel DI-KS 13, 35, 36.

#include "WorldTestSupport.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace GYO::Test::Kernel;

// This file's own types (their hash functions are seen wherever they are registered, DI-KS 27).
struct XPos { std::int32_t x; };
struct XVel { std::int32_t v; };
struct XLater { std::int32_t v; };
struct XRes { std::int64_t value; };
struct XOut { std::int64_t value; };
struct XNote { std::uint32_t n; };
struct XDrop { std::uint32_t n; };  // a message of capacity 1 (XK-6)
void HashValue(Hasher& h, const XPos& v) { HashValue(h, v.x); }
void HashValue(Hasher& h, const XVel& v) { HashValue(h, v.v); }
void HashValue(Hasher& h, const XLater& v) { HashValue(h, v.v); }
void HashValue(Hasher& h, const XRes& v) { HashValue(h, v.value); }
void HashValue(Hasher& h, const XOut& v) { HashValue(h, v.value); }
void HashValue(Hasher& h, const XNote& v) { HashValue(h, v.n); }
void HashValue(Hasher& h, const XDrop& v) { HashValue(h, v.n); }

void RegisterExtractData(Registrar& r) {
    r.Component<XPos>({.name = "XPos"});
    r.Component<XVel>({.name = "XVel"});
    r.Resource<XRes>({.name = "XRes"});
    r.Resource<XOut>({.name = "XOut"});
    r.Message<XNote>({.name = "XNote"});
    r.Message<XDrop>({.name = "XDrop", .capacity = 1});
}

bool Done(const GYO::Engine::Base::Result<void, StartupReport>& result) { return static_cast<bool>(result); }
bool Done(const GYO::Engine::Base::Result<void, KernelError>& result) { return static_cast<bool>(result); }

std::vector<std::string>& Seen() {
    static std::vector<std::string> seen;
    return seen;
}

ScheduleOptions ExtractSchedule() {
    ScheduleOptions s;
    s.name = "Extract";
    s.extract = true;
    s.stages = {"Copy", "After"};
    return s;
}

std::string PosText(EntityId id, const XPos& pos) {
    return std::to_string(id) + ":" + std::to_string(pos.x);
}

// Copies every source XPos into the reader (deferred) and the source XRes
// into the reader's XOut (now), logging what it read.
SystemResult CopyFromSource(Extract<Read<XPos>> pos, ExtractRes<XRes> res, Commands commands,
                            ResMut<XOut> out) {
    std::string line = "copy";
    pos.ForEach([&](EntityId id, const XPos& value) {
        line += " " + PosText(id, value);
        commands.Insert(id, value);
    });
    line += res.Present() ? " res:" + std::to_string(res->value) : std::string(" res:none");
    Seen().push_back(line);
    if (res.Present()) out->value = (*res).value;
    return Ok();
}

// The read methods of Extract<Read<T>> (XK-3).
SystemResult ReadMethods(Extract<Read<XPos>> pos, Extract<Read<XVel>> vel, ExtractRes<XOut> missing) {
    std::string line = "entities";
    for (EntityId id : pos.Entities()) line += " " + std::to_string(id);
    Seen().push_back(line);
    Seen().push_back("count " + std::to_string(pos.Count()) + " vel " + std::to_string(vel.Count()));
    Seen().push_back(std::string("contains 7:") + (pos.Contains(7) ? "yes" : "no") + " 8:" + (pos.Contains(8) ? "yes" : "no"));
    const XPos* seven = pos.Get(7);
    Seen().push_back("get 7:" + (seven != nullptr ? std::to_string(seven->x) : std::string("null")) +
                     " 8:" + (pos.Get(8) != nullptr ? std::string("value") : std::string("null")));
    Seen().push_back(std::string("missing res:") + (missing.Present() ? "present" : "absent"));
    return Ok();
}

// A run condition that reads the source (XK-4).
bool SourceFlagOn(ExtractRes<XRes> res) { return res.Present() && res->value > 0; }
SystemResult Guarded(Res<XOut>) {
    Seen().push_back("guarded ran");
    return Ok();
}
SystemResult UpdateGuarded(Res<XOut>) { return Ok(); }

// The reader's own messages within one RunExtract (XK-6).
SystemResult SendNote(Extract<Read<XPos>> pos, MessageWriter<XNote> writer) {
    writer.Send(XNote{static_cast<std::uint32_t>(pos.Count())});
    return Ok();
}
// Two writes to a buffer of one: one record dropped each RunExtract (XK-6).
SystemResult SendTwoDrops(MessageWriter<XDrop> writer) {
    writer.Send(XDrop{1});
    writer.Send(XDrop{2});
    return Ok();
}
SystemResult ReadNote(MessageReader<XNote> reader) {
    Seen().push_back("notes " + std::to_string(reader.Read().size()));
    return Ok();
}

SystemResult FailsInSource(Res<XRes>) {
    return GYO::Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "planned failure"));
}

// The source dynamically registers XLater later (XK-5).
SystemResult ReadsDerived(Extract<Read<XLater>> derived) {
    Seen().push_back("derived " + std::to_string(derived.Count()));
    return Ok();
}

// The source Registry: the common data and the Update schedule, built.
std::unique_ptr<Registry> MakeSource() {
    auto source = std::make_unique<Registry>();
    REQUIRE(Done(RegisterAll(*source, {{"source", [](Registrar& r) {
                                            RegisterExtractData(r);
                                            r.Schedule(UpdateSchedule());
                                        }}})));
    return source;
}

// A reader Registry whose extract source is source, with the common data, the
// extract schedule, the Update schedule and the given registrations.
std::unique_ptr<Registry> MakeReader(const Registry& source, std::function<void(Registrar&)> extra) {
    auto reader = std::make_unique<Registry>();
    reader->SetExtractSource(source);
    const auto built = RegisterAll(*reader, {{"reader", [&](Registrar& r) {
                                                  RegisterExtractData(r);
                                                  r.Schedule(ExtractSchedule());
                                                  r.Schedule(UpdateSchedule());
                                                  extra(r);
                                              }}});
    REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().detail));
    return reader;
}

} // namespace

TEST_CASE("DI-KS 35: RunExtract reads the source World's values; the reader keeps its own") {
    Seen().clear();
    auto sourceRegistry = MakeSource();
    auto readerRegistry = MakeReader(*sourceRegistry, [](Registrar& r) {
        r.AddSystem<&CopyFromSource>(Sys("CopyFromSource", {At("Extract", "Copy")}));
    });
    WorldExecutor source(*sourceRegistry, "source");
    WorldExecutor reader(*readerRegistry, "reader");
    // The two Worlds hold different values of the same types: reading the wrong World shows.
    source.GetWorld().Insert(5, XPos{50});
    source.GetWorld().Insert(9, XPos{90});
    source.GetWorld().SetResource(XRes{11});
    reader.GetWorld().Insert(5, XPos{-1});
    reader.GetWorld().Insert(9, XVel{0});  // alive in the reader, so the deferred Insert lands
    reader.GetWorld().SetResource(XRes{-7});
    reader.GetWorld().SetResource(XOut{0});

    const auto first = reader.RunExtract("Extract", source.GetWorld());
    REQUIRE_MESSAGE(static_cast<bool>(first), Text(first));
    CHECK_MESSAGE(Seen() == std::vector<std::string>{"copy 5:50 9:90 res:11"}, "XK-1: the extract System read the source");
    // Written to the reader: now (ResMut) and at the reader's sync point (Commands).
    CHECK(reader.GetWorld().Resource<XOut>()->value == 11);
    REQUIRE(reader.GetWorld().Get<XPos>(9) != nullptr);
    CHECK(reader.GetWorld().Get<XPos>(5)->x == 50);
    CHECK(reader.GetWorld().Get<XPos>(9)->x == 90);

    // The source changes; the next extract reads the new values.
    *source.GetWorld().GetMut<XPos>(5) = XPos{55};
    source.GetWorld().SetResource(XRes{12});
    Seen().clear();
    REQUIRE(static_cast<bool>(reader.RunExtract("Extract", source.GetWorld())));
    CHECK_MESSAGE(Seen() == std::vector<std::string>{"copy 5:55 9:90 res:12"}, "XK-1: the next extract reads the new values");
}

TEST_CASE("DI-KS 36: RunExtract leaves the source World as it was") {
    Seen().clear();
    auto sourceRegistry = MakeSource();
    auto readerRegistry = MakeReader(*sourceRegistry, [](Registrar& r) {
        r.AddSystem<&CopyFromSource>(Sys("CopyFromSource", {At("Extract", "Copy")}));
    });
    WorldExecutor source(*sourceRegistry, "source");
    WorldExecutor reader(*readerRegistry, "reader");
    source.GetWorld().Insert(3, XPos{30});
    source.GetWorld().SetResource(XRes{4});
    reader.GetWorld().SetResource(XOut{0});
    reader.GetWorld().Insert(3, XVel{0});
    REQUIRE(static_cast<bool>(source.Run("Update")));

    const Fingerprint before = SummaryOf(source.GetWorld());
    const auto stampsBefore = source.GetWorld().Stamps<XPos>(3);
    REQUIRE(static_cast<bool>(reader.RunExtract("Extract", source.GetWorld())));
    REQUIRE(static_cast<bool>(reader.RunExtract("Extract", source.GetWorld())));
    CHECK_MESSAGE(SummaryOf(source.GetWorld()) == before, "XK-2: the source summary is unchanged");
    CHECK_MESSAGE(source.GetWorld().Stamps<XPos>(3) == stampsBefore, "XK-2: the source cell's stamps are unchanged");
    CHECK(source.GetWorld().State() == WorldState::Ready);
    CHECK(reader.GetWorld().Get<XPos>(3) != nullptr);
}

TEST_CASE("DI-KS 36: the read methods of Extract parameters") {
    Seen().clear();
    auto sourceRegistry = MakeSource();
    auto readerRegistry = MakeReader(*sourceRegistry, [](Registrar& r) {
        r.AddSystem<&ReadMethods>(Sys("ReadMethods", {At("Extract", "Copy")}));
    });
    WorldExecutor source(*sourceRegistry, "source");
    WorldExecutor reader(*readerRegistry, "reader");
    // Put in out of order: Entities and ForEach are ascending.
    source.GetWorld().Insert(12, XPos{120});
    source.GetWorld().Insert(7, XPos{70});
    source.GetWorld().Insert(3, XPos{30});
    source.GetWorld().Insert(8, XVel{1});

    REQUIRE(static_cast<bool>(reader.RunExtract("Extract", source.GetWorld())));
    REQUIRE(Seen().size() == 5);
    CHECK_MESSAGE(Seen()[0] == "entities 3 7 12", "XK-3: Entities ascending: " << Seen()[0]);
    CHECK_MESSAGE(Seen()[1] == "count 3 vel 1", "XK-3: Count: " << Seen()[1]);
    CHECK_MESSAGE(Seen()[2] == "contains 7:yes 8:no", "XK-3: Contains: " << Seen()[2]);
    CHECK_MESSAGE(Seen()[3] == "get 7:70 8:null", "XK-3: Get: " << Seen()[3]);
    CHECK_MESSAGE(Seen()[4] == "missing res:absent", "XK-3: ExtractRes::Present: " << Seen()[4]);
}

TEST_CASE("DI-KS 35: a run condition of an extract schedule reads the source") {
    Seen().clear();
    auto sourceRegistry = MakeSource();
    auto readerRegistry = MakeReader(*sourceRegistry, [](Registrar& r) {
        SystemOptions guarded = Sys("Guarded", {At("Extract", "Copy")});
        guarded.mounts[0].conditions.push_back(Condition<&SourceFlagOn>("SourceFlagOn"));
        r.AddSystem<&Guarded>(guarded);
    });
    WorldExecutor source(*sourceRegistry, "source");
    WorldExecutor reader(*readerRegistry, "reader");
    reader.GetWorld().SetResource(XOut{0});
    REQUIRE(static_cast<bool>(reader.RunExtract("Extract", source.GetWorld())));
    CHECK_MESSAGE(Seen().empty(), "XK-4: the source has no XRes: the condition is false");
    source.GetWorld().SetResource(XRes{1});
    REQUIRE(static_cast<bool>(reader.RunExtract("Extract", source.GetWorld())));
    CHECK_MESSAGE(Seen() == std::vector<std::string>{"guarded ran"}, "XK-4: the condition read the source's XRes");
}

TEST_CASE("DI-KS 36: the source attaches to its Registry's newer version when an extract reads it") {
    Seen().clear();
    auto sourceRegistry = MakeSource();
    auto readerRegistry = MakeReader(*sourceRegistry, [](Registrar&) {});
    WorldExecutor source(*sourceRegistry, "source");
    WorldExecutor reader(*readerRegistry, "reader");
    REQUIRE(static_cast<bool>(source.Run("Update")));
    // The source registers a new component; the source World is not used again before the extract.
    REQUIRE(Done(sourceRegistry->Register({"later", 1}, [](Registrar& r) { r.Component<XLater>({.name = "XLater"}); })));
    REQUIRE(Done(sourceRegistry->BuildPlans()));
    REQUIRE(Done(readerRegistry->Register({"later", 1}, [](Registrar& r) {
        r.AddSystem<&ReadsDerived>(Sys("ReadsDerived", {At("Extract", "Copy")}));
    })));
    REQUIRE(Done(readerRegistry->BuildPlans()));
    REQUIRE(static_cast<bool>(reader.RunExtract("Extract", source.GetWorld())));
    CHECK_MESSAGE(Seen() == std::vector<std::string>{"derived 0"}, "XK-5: the new column of the source is read (empty)");
}

TEST_CASE("DI-KS 35: RunExtract is an outermost Run of the reader: its own messages are trimmed") {
    Seen().clear();
    auto sourceRegistry = MakeSource();
    auto readerRegistry = MakeReader(*sourceRegistry, [](Registrar& r) {
        r.AddSystem<&SendNote>(Sys("SendNote", {At("Extract", "Copy")}));
        r.AddSystem<&ReadNote>(Sys("ReadNote", {At("Extract", "After")}));
    });
    WorldExecutor source(*sourceRegistry, "source");
    WorldExecutor reader(*readerRegistry, "reader");
    REQUIRE(static_cast<bool>(reader.RunExtract("Extract", source.GetWorld())));
    CHECK(Seen() == std::vector<std::string>{"notes 1"});
    CHECK_MESSAGE(reader.GetWorld().MessagePayloads<XNote>().empty(),
                  "XK-6: the record every reader read is trimmed when RunExtract ends");
    CHECK_MESSAGE(reader.GetWorld().HasRun(), "XK-6: RunExtract reads KernelSettings as a Run does");
}

TEST_CASE("DI-KS 35: RunExtract reads KernelSettings when it starts and keeps its own DroppedMessages records") {
    auto sourceRegistry = MakeSource();
    auto readerRegistry = MakeReader(*sourceRegistry, [](Registrar& r) {
        r.AddSystem<&SendTwoDrops>(Sys("SendTwoDrops", {At("Extract", "Copy")}));
    });
    WorldExecutor source(*sourceRegistry, "source");
    WorldExecutor reader(*readerRegistry, "reader");
    // The switch written between two Runs counts from the next RunExtract (J-50).
    reader.GetWorld().SetResource(KernelSettings{true, false});
    REQUIRE(static_cast<bool>(reader.RunExtract("Extract", source.GetWorld())));
    CHECK_MESSAGE(reader.Verification().accessVerification, "XK-6: RunExtract read KernelSettings when it started");
    reader.GetWorld().SetResource(KernelSettings{false, false});
    REQUIRE(static_cast<bool>(reader.RunExtract("Extract", source.GetWorld())));
    CHECK_MESSAGE(!reader.Verification().accessVerification, "XK-6: RunExtract read KernelSettings again");
    // Each outermost RunExtract records its own drop (event cursor declaration 3.2), not one merged record.
    CHECK_MESSAGE(reader.GetWorld().Diagnostics<DroppedMessages>().size() == 2,
                  "XK-6: one DroppedMessages record per RunExtract, got " << reader.GetWorld().Diagnostics<DroppedMessages>().size());
}

TEST_CASE("DI-KS 35: a run condition with Extract parameters outside an extract schedule is D-E7") {
    const auto sourceRegistry = MakeSource();
    const auto build = [&](bool extractMount, bool setCondition) {
        Built built{std::make_unique<Registry>(), {}};
        built.registry->SetExtractSource(*sourceRegistry);
        built.result = RegisterAll(*built.registry, {{"reader", [&](Registrar& r) {
                                                          RegisterExtractData(r);
                                                          r.Schedule(ExtractSchedule());
                                                          r.Schedule(UpdateSchedule());
                                                          const std::string schedule = extractMount ? "Extract" : "Update";
                                                          const std::string stage = extractMount ? "Copy" : "First";
                                                          SystemOptions options = Sys("UpdateGuarded", {At(schedule, stage)});
                                                          if (setCondition) {
                                                              r.Set({.name = "FromSource", .schedule = schedule},
                                                                    {Condition<&SourceFlagOn>("SourceFlagOn")});
                                                              options.mounts[0].sets = {"FromSource"};
                                                          } else {
                                                              options.mounts[0].conditions.push_back(
                                                                  Condition<&SourceFlagOn>("SourceFlagOn"));
                                                          }
                                                          r.AddSystem<&UpdateGuarded>(options);
                                                      }}});
        return built;
    };
    const Built mountCondition = build(false, false);
    CHECK_MESSAGE(HasRule(mountCondition, StartupCode::DE7, "H-6"), "XK-7: a mount's condition: " << Describe(mountCondition));
    const Built setCondition = build(false, true);
    CHECK_MESSAGE(HasRule(setCondition, StartupCode::DE7, "H-6"), "XK-7: a set's condition: " << Describe(setCondition));
    const Built onExtract = build(true, false);
    CHECK_MESSAGE(static_cast<bool>(onExtract.result), "XK-7: the same condition on an extract schedule: " << Describe(onExtract));
    const Built setOnExtract = build(true, true);
    CHECK_MESSAGE(static_cast<bool>(setOnExtract.result), "XK-7: a set condition on an extract schedule: " << Describe(setOnExtract));
}

TEST_CASE("DI-KS 35: the asserts of RunExtract and Run") {
    auto sourceRegistry = MakeSource();
    auto readerRegistry = MakeReader(*sourceRegistry, [](Registrar&) {});
    auto otherRegistry = MakeSource();
    WorldExecutor source(*sourceRegistry, "source");
    WorldExecutor other(*otherRegistry, "other");
    WorldExecutor reader(*readerRegistry, "reader");
    // Run names an extract schedule; RunExtract names one that is not.
    GYO_CHECK_ASSERTS(reader.Run("Extract"));
    GYO_CHECK_ASSERTS(reader.RunExtract("Update", source.GetWorld()));
    // The source is a World of another Registry, or the reader itself.
    GYO_CHECK_ASSERTS(reader.RunExtract("Extract", other.GetWorld()));
    GYO_CHECK_ASSERTS(reader.RunExtract("Extract", reader.GetWorld()));
    // The reader and the source are still usable after the refused calls.
    CHECK(static_cast<bool>(reader.RunExtract("Extract", source.GetWorld())));

    // The source is not Ready (interrupted).
    Registry failingRegistry;
    REQUIRE(Done(RegisterAll(failingRegistry, {{"source", [](Registrar& r) {
                                                    RegisterExtractData(r);
                                                    r.Schedule(UpdateSchedule());
                                                    r.AddSystem<&FailsInSource>(Sys("FailsInSource", {At("Update", "First")}));
                                                }}})));
    auto failingReader = MakeReader(failingRegistry, [](Registrar&) {});
    WorldExecutor failing(failingRegistry, "failing");
    WorldExecutor readsFailing(*failingReader, "reads-failing");
    failing.GetWorld().SetResource(XRes{1});
    REQUIRE_FALSE(static_cast<bool>(failing.Run("Update")));
    REQUIRE(failing.GetWorld().State() == WorldState::Interrupted);
    GYO_CHECK_ASSERTS(readsFailing.RunExtract("Extract", failing.GetWorld()));
}
