#include "doctest/doctest.h"

#include "engine/base/IdIssuer.hpp"

#include <cstddef>
#include <cstdint>
#include <set>
#include <thread>
#include <vector>

using GYO::Engine::Base::CurrentThreadSerial;
using GYO::Engine::Base::IdIssuer;

TEST_CASE("IdIssuer: numbers start at 1, never repeat and never are 0") {
    IdIssuer issuer;
    std::set<std::uint64_t> seen;
    for (int i = 0; i < 1000; ++i) {
        const std::uint64_t id = issuer.Next();
        CHECK_MESSAGE(id != 0, "[id-issuer-zero] Next() returned 0");
        CHECK_MESSAGE(seen.insert(id).second, "[id-issuer-repeat] Next() returned " << id << " twice");
    }
    CHECK_MESSAGE(*seen.begin() == 1, "[id-issuer-first] the first number is " << *seen.begin());
}

TEST_CASE("IdIssuer: two issuers are separate sources") {
    IdIssuer first;
    IdIssuer second;
    CHECK(first.Next() == 1);
    CHECK(first.Next() == 2);
    CHECK_MESSAGE(second.Next() == 1, "[id-issuer-shared] a second issuer continued the first one's numbers");
}

TEST_CASE("IdIssuer: Next() from several threads at once never hands one number out twice") {
    IdIssuer issuer;
    constexpr std::size_t threads = 8;
    constexpr std::size_t each = 10000;
    std::vector<std::vector<std::uint64_t>> got(threads);
    std::vector<std::thread> workers;
    for (std::size_t t = 0; t < threads; ++t) {
        workers.emplace_back([&issuer, &got, t] {
            got[t].reserve(each);
            for (std::size_t i = 0; i < each; ++i) got[t].push_back(issuer.Next());
        });
    }
    for (std::thread& worker : workers) worker.join();
    std::set<std::uint64_t> all;
    for (const auto& numbers : got) all.insert(numbers.begin(), numbers.end());
    CHECK_MESSAGE(all.size() == threads * each, "[id-issuer-race] " << threads * each - all.size() << " numbers handed out twice");
}

TEST_CASE("CurrentThreadSerial: the same number on one thread, never 0") {
    const std::uint64_t serial = CurrentThreadSerial();
    CHECK_MESSAGE(serial != 0, "[thread-serial-zero] the thread's serial is 0");
    CHECK_MESSAGE(CurrentThreadSerial() == serial, "[thread-serial-changes] a second call gave another number");
}

TEST_CASE("CurrentThreadSerial: a thread started after another ended does not get its number") {
    // The case a thread_local address misses on macOS: one thread at a time,
    // each ended before the next starts, so the runtime may reuse its storage.
    std::set<std::uint64_t> serials{CurrentThreadSerial()};
    constexpr int rounds = 200;
    for (int i = 0; i < rounds; ++i) {
        std::uint64_t serial = 0;
        std::uint64_t again = 0;
        std::thread([&serial, &again] {
            serial = CurrentThreadSerial();
            again = CurrentThreadSerial();
        }).join();
        CHECK_MESSAGE(serial != 0, "[thread-serial-zero] round " << i << " got 0");
        CHECK_MESSAGE(again == serial, "[thread-serial-changes] round " << i);
        CHECK_MESSAGE(serials.insert(serial).second, "[thread-serial-reused] round " << i << " got " << serial << " again");
    }
}
