#include "engine/logging/Span.hpp"

#include <span>

#include "LoggerState.hpp"

namespace GYO::Engine::Logging {

Span::Span(const std::string_view category, const std::string_view text, const Level level) noexcept
    : category_(category), text_(text), level_(level), active_(false) {
    const Detail::LoggerState* state = Detail::current.load(std::memory_order_acquire);
    if (state == nullptr) {
        // No Logger: the span goes to standard error, as Write's records do.
        active_ = true;
        beginNs_ = Detail::DefaultMonoNs();
        return;
    }
    // A span no sink takes reads no clock (LG-9).
    active_ = Detail::TakingSinks(*state, level, category) != 0;
    if (active_) beginNs_ = state->MonoNs();
}

Span::~Span() {
    End();
}

void Span::End(const std::initializer_list<Field> fields) noexcept {
    if (ended_) return;
    ended_ = true;
    if (!active_) return;
    const Detail::LoggerState* state = Detail::current.load(std::memory_order_acquire);
    const std::int64_t endNs = state != nullptr ? state->MonoNs() : Detail::DefaultMonoNs();
    Record record{.kind = RecordKind::Span,
                  .level = level_,
                  .category = category_,
                  .text = text_,
                  .wall = state != nullptr ? state->Wall() : Detail::DefaultWall(),
                  .monoNs = endNs,
                  .fields = std::span<const Field>(fields.begin(), fields.size()),
                  .beginNs = beginNs_,
                  .durationNs = endNs - beginNs_};
    if (state == nullptr) {
        Detail::WriteWithoutLogger(record);
        return;
    }
    // The filter may have changed since the span began (SetLevel): the sinks that take it now.
    const std::uint64_t taking = Detail::TakingSinks(*state, level_, category_);
    if (taking != 0) Detail::DeliverRecord(*state, taking, record);
}

} // namespace GYO::Engine::Logging
