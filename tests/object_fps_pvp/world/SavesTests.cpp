// The Match's saves (gateway declaration D7, segment 4): a Client connects, the Match reads its save from the save
// service, the Client leaves, the Match writes the settled save back. The save service is a loopback listener that
// answers as the Go service does (tests/common/os/LoopbackListener.hpp keeps the native socket code in tests/common/os).

#include "WorldTestSupport.hpp"

#include "object_fps_pvp/world/MatchSaves.hpp"

#include "LoopbackListener.hpp"

#include <chrono>
#include <string>
#include <thread>

namespace {

using namespace object_fps_pvp::test;

std::unique_ptr<O::Product> BuildMatchWithSaves(std::shared_ptr<N::Link> link, std::shared_ptr<PW::Seats> seats,
                                                std::function<N::NetTime()> clock, const PW::MatchSavesOptions saves) {
    O::ProductDescription description;
    description.name = "pvp-match";
    N::NetProductOptions net;
    net.role = N::NetRole::Server;
    net.link = std::move(link);
    net.clock = std::move(clock);
    net.seed = 7;
    net.admission = PW::SeatAdmission(seats);
    description.options.Set(net);
    description.worlds.push_back(PW::WithMatchSaves(Stepped(PW::MatchWorld({ShippedContent(), 0, 0, seats})), saves));
    auto built = O::Product::Build(std::move(description), O::MakeRegisteredUnits());
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    return std::move(built).value();
}

std::string Answer(const std::string& body) {
    return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) +
           "\r\n\r\n" + body;
}

} // namespace

TEST_CASE("world: the Match reads a player's save when they join and writes it back, settled, when they leave") {
    GYO::Test::LoopbackListener service;
    std::string read;
    std::string write;
    std::thread answering([&] {
        service.Accept();
        read = service.ReceiveRequest();
        service.Send(Answer("{\"userId\":1,\"version\":3,\"body\":{\"saveVersion\":1,\"matches\":2,\"deaths\":1}}"));
        service.CloseAccepted();
        service.Accept();
        write = service.ReceiveRequest();
        service.Send(Answer("{\"version\":4}"));
        service.CloseAccepted();
    });

    auto clock = std::make_shared<N::NetTime>(0);
    auto reader = [clock] { return *clock; };
    N::MemoryLinkSettings settings;
    settings.seed = 61;
    auto links = N::MemoryLink::Pair(settings);
    auto seats = std::make_shared<PW::Seats>(2);
    auto match = BuildMatchWithSaves(std::make_shared<N::MemoryLink>(links.first), seats, reader,
                                     PW::MatchSavesOptions{service.Address(), 77});
    auto client = BuildClient(ShippedContent(), std::make_shared<N::MemoryLink>(links.second), 1, reader);
    const auto frame = [&] {
        REQUIRE(static_cast<bool>(match->RunFrame()));
        if (client) REQUIRE(static_cast<bool>(client->RunFrame()));
        links.first.Advance();
        *clock += std::chrono::milliseconds(16);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    };
    const auto report = [&] { return GameWorld(*match).Resource<PW::MatchSaves>()->Report(); };
    const auto until = [&](auto done, const char* what) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!done() && std::chrono::steady_clock::now() < deadline) frame();
        CHECK_MESSAGE(done(), what << " (failures " << report().failures << ": " << report().lastFailure << ")");
    };

    until([&] { return report().reads == 1; }, "the joining player's save was never read");
    auto* net = client->Buffer().Find<N::NetService>();
    REQUIRE(net != nullptr);
    net->Close();
    for (int i = 0; i < 10; ++i) frame();
    client.reset();
    until([&] { return report().writes == 1; }, "the leaving player's save was never written");
    answering.join();

    CHECK(read.rfind("GET /internal/v1/saves/1 HTTP/1.1\r\n", 0) == 0);
    CHECK(write.rfind("PUT /internal/v1/saves/1 HTTP/1.1\r\n", 0) == 0);
    CHECK_MESSAGE(write.find("Idempotency-Key: match-77-user-1-leave-1\r\n") != std::string::npos, write);
    CHECK_MESSAGE(write.find("{\"version\":3,\"body\":{\"saveVersion\":1,\"matches\":3,\"deaths\":1}}") != std::string::npos,
                  "the settled save is not the one read plus one match: " << write);
    CHECK(report().failures == 0);
}
