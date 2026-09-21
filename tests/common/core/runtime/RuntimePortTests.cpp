#include "doctest/doctest.h"

#include "engine/runtime/IRuntimePort.hpp"

#include <deque>
#include <span>
#include <vector>

namespace {

struct Snapshot {
    int value{};
};

enum class Command {
    Increment,
};

enum class Event {
    Incremented,
};

class FakePort final
    : public Engine::Runtime::IRuntimePort<Snapshot, Command, Event> {
public:
    const Snapshot& Query() const noexcept override { return snapshot_; }

    void Submit(const Command command) override { commands_.push_back(command); }

    std::span<const Event> Events() const noexcept override { return events_; }

    void Advance() {
        events_.clear();
        while (!commands_.empty()) {
            if (commands_.front() == Command::Increment) {
                ++snapshot_.value;
                events_.push_back(Event::Incremented);
            }
            commands_.pop_front();
        }
    }

private:
    Snapshot snapshot_{};
    std::deque<Command> commands_;
    std::vector<Event> events_;
};

} // namespace

TEST_CASE("IRuntimePort exposes typed query, command, and event seams") {
    FakePort port;
    Engine::Runtime::IRuntimePort<Snapshot, Command, Event>& boundary = port;

    boundary.Submit(Command::Increment);
    CHECK(boundary.Query().value == 0);

    port.Advance();
    CHECK(boundary.Query().value == 1);
    REQUIRE(boundary.Events().size() == 1);
    CHECK(boundary.Events().front() == Event::Incremented);
}
