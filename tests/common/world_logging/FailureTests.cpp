// Entrance ⑤ (logging declaration 5.6; Design Invariant LG-2): the runner or main hands the WorldFailure of
// a broken World to WriteFailure, which writes one Fatal record of category "world" from the failure's
// values only; the runner then exits (user, 2026-11-20: "log it, then exit"; nothing is de-duplicated).
#include <doctest/doctest.h>

// MSVC needs <ostream> for doctest to print std::string_view in CHECK.
#include <ostream>

#include <cstdint>
#include <stdexcept>
#include <string>

#include "WorldLoggingTestSupport.hpp"
#include "engine/world_logging/TakeKernelDiagnostics.hpp"
#include "engine/world_logging/WriteFailure.hpp"

namespace WL = GYO::Engine::WorldLogging;
using namespace GYO::Engine::WorldLogging::Test;

namespace {

enum class Breaks : std::int32_t { No = 0, ReturnsErr, Throws };

struct BreakNow final {
    std::int32_t how = 0;
};
struct Crowd final {
    std::int32_t value = 0;
};

K::SystemResult MaybeBreak(K::Res<BreakNow> now, K::MessageWriter<Crowd> crowd) {
    // Over the capacity of 1: the World also has a dropped-messages record when it breaks.
    crowd.Send(Crowd{1});
    crowd.Send(Crowd{2});
    if (now->how == static_cast<std::int32_t>(Breaks::ReturnsErr)) {
        return ::Engine::Base::Err(K::KernelError(K::KernelErrorCode::SystemFailed, "MaybeBreak gave up"));
    }
    if (now->how == static_cast<std::int32_t>(Breaks::Throws)) throw std::runtime_error("MaybeBreak threw");
    return {};
}

O::ProductDescription Breakable(O::Runner runner = {}) {
    O::ProductDescription d;
    d.name = "game";
    O::WorldDescription w;
    w.name = "Simulation";
    w.build = [](O::WorldSetup& s) {
        K::Registrar& r = s.Registrar();
        r.Resource<BreakNow>({.name = "BreakNow", .zone = K::Zone::B});
        r.Message<Crowd>({.name = "Crowd", .zone = K::Zone::B, .capacity = 1});
        r.AddSystem<&MaybeBreak>(At("MaybeBreak", "Main", "Update"));
    };
    w.prepare.push_back([](K::World& world) -> ::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(BreakNow{});
        return {};
    });
    d.worlds.push_back(std::move(w));
    if (runner) {
        d.configure = [runner = std::move(runner)](O::ProductSetup& setup) { setup.InstallRunner("test", runner); };
    }
    return d;
}

void BreakNext(O::Product& product, Breaks how) {
    product.World("Simulation").GetWorld().SetResource(BreakNow{static_cast<std::int32_t>(how)});
}

std::vector<L::StoredRecord> Fatal(const Logged& logged) {
    std::vector<L::StoredRecord> out;
    for (auto& record : logged.memory->Records()) {
        if (record.fatal) out.push_back(std::move(record));
    }
    return out;
}

} // namespace

TEST_CASE("failure: an entry point's error: one Fatal record of category world from the WorldFailure's values") {
    Logged logged;
    auto product = Make(Breakable());
    Frame(*product);
    BreakNext(*product, Breaks::ReturnsErr);
    auto ran = product->RunFrame();
    REQUIRE_FALSE(ran);
    REQUIRE(ran.error().failure.has_value());
    const O::WorldFailure& failure = *ran.error().failure;
    const auto report = WL::WriteFailure(failure);
    CHECK(report.console);
    CHECK(report.sinksWritten == 1);

    const auto records = Fatal(logged);
    REQUIRE(records.size() == 1);
    const auto& record = records[0];
    CHECK(record.level == L::Level::Fatal);
    CHECK(record.category == "world");
    CHECK(record.product == "game");
    CHECK(record.world == "Simulation");
    CHECK(record.text == failure.Text());
    REQUIRE(record.Find("kind") != nullptr);
    CHECK(record.Find("kind")->text == "Interrupted");
    CHECK(record.Find("entry")->text == "RunFrame");
    CHECK(record.Find("schedule")->text == "Main");
    CHECK(record.Find("sync_epoch")->uintValue == failure.syncEpoch);
    CHECK(record.Find("kernel_code")->text == "SystemFailed");
    CHECK(record.Find("where")->text == failure.where);
    CHECK(record.Find("last_schedule")->text == failure.lastSchedule);
}

TEST_CASE("failure: a System that throws: RunFrame throws; Product::Failure() is written") {
    Logged logged;
    auto product = Make(Breakable());
    BreakNext(*product, Breaks::Throws);
    bool threw = false;
    try {
        static_cast<void>(product->RunFrame());
    } catch (const std::runtime_error&) {
        threw = true;
        REQUIRE(product->Failure() != nullptr);
        CHECK(WL::WriteFailure(*product->Failure()).sinksWritten == 1);
    }
    CHECK(threw);
    const auto records = Fatal(logged);
    REQUIRE(records.size() == 1);
    CHECK(records[0].Find("kind")->text == "Interrupted");
    CHECK(records[0].Find("kernel_code") == nullptr);
}

TEST_CASE("failure: WithFailureLog: the wrapped runner returns on the failure; it is written once and the exit is Failure") {
    Logged logged;
    int framesAfterFailure = 0;
    O::Runner loop = [&framesAfterFailure](O::Product& product) -> O::RunnerExit {
        for (int frame = 0; frame < 10; ++frame) {
            if (frame == 2) BreakNext(product, Breaks::ReturnsErr);
            if (product.Failure() != nullptr) ++framesAfterFailure;
            if (!product.RunFrame()) return {O::RunnerExitCode::Failure, ""};
        }
        return {};
    };
    auto product = Make(Breakable(WL::WithFailureLog(std::move(loop))));
    const auto exit = product->Run();
    CHECK(exit.code == O::RunnerExitCode::Failure);
    CHECK(exit.reason == product->Failure()->Text());
    CHECK(framesAfterFailure == 0);
    CHECK(Fatal(logged).size() == 1);
}

TEST_CASE("failure: WithFailureLog: a runner that ends Success while the Product has a failure still ends Failure") {
    Logged logged;
    O::Runner careless = [](O::Product& product) -> O::RunnerExit {
        BreakNext(product, Breaks::ReturnsErr);
        static_cast<void>(product.RunFrame());
        return {O::RunnerExitCode::Success, "ignored the error"};
    };
    auto product = Make(Breakable(WL::WithFailureLog(std::move(careless))));
    const auto exit = product->Run();
    CHECK(exit.code == O::RunnerExitCode::Failure);
    CHECK(Fatal(logged).size() == 1);
}

TEST_CASE("failure: WithFailureLog: an exception that broke the World is written and becomes a Failure exit") {
    Logged logged;
    O::Runner throwing = [](O::Product& product) -> O::RunnerExit {
        BreakNext(product, Breaks::Throws);
        static_cast<void>(product.RunFrame());
        return {};
    };
    auto product = Make(Breakable(WL::WithFailureLog(std::move(throwing))));
    O::RunnerExit exit;
    CHECK_NOTHROW(exit = product->Run());
    CHECK(exit.code == O::RunnerExitCode::Failure);
    CHECK(Fatal(logged).size() == 1);
}

TEST_CASE("failure: WithFailureLog: an exception with no broken World is thrown on and nothing is written") {
    Logged logged;
    O::Runner throwing = [](O::Product&) -> O::RunnerExit { throw std::logic_error("the runner's own bug"); };
    auto product = Make(Breakable(WL::WithFailureLog(std::move(throwing))));
    CHECK_THROWS_AS(product->Run(), std::logic_error);
    CHECK(Fatal(logged).empty());
}

TEST_CASE("failure: a healthy run writes nothing; WithFailureLog passes the runner's exit through") {
    Logged logged;
    O::Runner fine = [](O::Product& product) -> O::RunnerExit {
        Frame(product);
        return {O::RunnerExitCode::Success, "done"};
    };
    auto product = Make(Breakable(WL::WithFailureLog(std::move(fine))));
    const auto exit = product->Run();
    CHECK(exit.code == O::RunnerExitCode::Success);
    CHECK(exit.reason == "done");
    CHECK(Fatal(logged).empty());
}

TEST_CASE("failure: WriteFailure first, then TakeKernelDiagnostics: the fatal record comes before the kernel's") {
    Logged logged;
    auto product = Make(Breakable());
    Frame(*product);
    BreakNext(*product, Breaks::ReturnsErr);
    auto ran = product->RunFrame();
    REQUIRE_FALSE(ran);
    static_cast<void>(WL::WriteFailure(*ran.error().failure));
    CHECK(WL::TakeKernelDiagnostics(product->World("Simulation").GetWorld()) >= 1);
    const auto records = logged.memory->Records();
    REQUIRE(records.size() >= 2);
    CHECK(records[0].fatal);
    CHECK(records[1].category == "kernel");
    CHECK(records[1].worldInterrupted);
}
