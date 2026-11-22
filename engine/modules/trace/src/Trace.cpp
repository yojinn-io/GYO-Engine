#include "engine/trace/Trace.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

#include "engine/base/Assert.hpp"
#include "engine/base/Trace.hpp"
#include "engine/logging/Logger.hpp"
#include "engine/logging/Record.hpp"
#include "engine/logging/Write.hpp"
#include "engine/time/MonotonicClock.hpp"

namespace GYO::Engine::Trace {

namespace {

namespace Logging = GYO::Engine::Logging;
using Logging::Field;
using Logging::Level;

// A stack deeper than this keeps counting but stores no more frames (their spans are not written).
constexpr std::size_t kMaxDepth = 64;
// An error text longer than this is cut.
constexpr std::size_t kErrorBytes = 256;
// "span." and a category of at most kMaxCategory bytes.
constexpr std::size_t kMaxCategory = 64;
constexpr std::string_view kSpanPrefix = "span.";
constexpr std::string_view kPathCategory = "span.path";
constexpr std::string_view kUnwoundText = "an exception left the span";
// The path record's path; a deeper one keeps its deepest end.
constexpr std::size_t kPathBytes = 1024;

struct Frame {
    std::uint64_t id = 0;
    std::uint64_t parent = 0;
    std::string_view category;
    std::string_view name;
    std::string_view world;
    std::int64_t beginNs = 0;
    bool timed = false;
    bool hasError = false;
    bool exception = false;
    // A span above this one ended by an exception (so this one is not the first the exception unwound).
    bool childUnwound = false;
    std::size_t errorSize = 0;
    char error[kErrorBytes]{};
};

struct Stack {
    std::uint32_t generation = 0;
    // Every begun span counts, also the ones past kMaxDepth that are not stored.
    std::size_t depth = 0;
    Frame frames[kMaxDepth];
};

// Constant-initialized: no first-use guard on the span path.
constinit thread_local Stack stack{};

std::atomic<bool> installed{false};
std::atomic<std::uint32_t> generation{0};
std::atomic<std::uint64_t> nextId{1};
std::int64_t (*clock)() noexcept = nullptr;
const Base::TraceReceiver* previousReceiver = nullptr;

std::int64_t Now() noexcept {
    return clock != nullptr ? clock() : ::GYO::Engine::Time::NowNs();
}

// This thread's stack under the current Install; a stack left by an earlier one is emptied first.
Stack& CurrentStack(const std::uint32_t current) noexcept {
    if (stack.generation != current) {
        stack.generation = current;
        stack.depth = 0;
    }
    return stack;
}

// "span." + category in buffer (no allocation); a category longer than kMaxCategory is cut, as an error text
// is (TR-5): the mechanism tells, it does not stop the program over a long name.
std::string_view SpanCategory(const std::string_view category, char (&buffer)[kSpanPrefix.size() + kMaxCategory]) noexcept {
    const std::size_t size = category.size() < kMaxCategory ? category.size() : kMaxCategory;
    std::memcpy(buffer, kSpanPrefix.data(), kSpanPrefix.size());
    std::memcpy(buffer + kSpanPrefix.size(), category.data(), size);
    return std::string_view(buffer, kSpanPrefix.size() + size);
}

// Whether a record of this category would be written now: only with a Logger installed and a sink taking it.
bool Taken(const std::string_view category) noexcept {
    return Logging::Installed() && Logging::Enabled(Level::Trace, category);
}

std::size_t StoredCount(const Stack& s) noexcept {
    return s.depth < kMaxDepth ? s.depth : kMaxDepth;
}

// The index of the stored frame with id, or kMaxDepth.
std::size_t Find(const Stack& s, const std::uint64_t id) noexcept {
    for (std::size_t i = StoredCount(s); i > 0; --i) {
        if (s.frames[i - 1].id == id) return i - 1;
    }
    return kMaxDepth;
}

// The path of frames [0, count): "category:name@world > ...", written right to left so that the deepest end
// is kept; "..." in front when something was left out. Returns the length, at the start of out.
std::size_t BuildPath(const Stack& s, const std::size_t count, const std::span<char> out) noexcept {
    if (out.empty() || count == 0) return 0;
    constexpr std::string_view kSeparator = " > ";
    constexpr std::string_view kCut = "...";
    std::size_t pos = out.size();
    bool cut = false;
    auto put = [&](const std::string_view text) {
        pos -= text.size();
        std::memcpy(out.data() + pos, text.data(), text.size());
    };
    for (std::size_t i = count; i > 0; --i) {
        const Frame& f = s.frames[i - 1];
        const std::size_t segment = f.category.size() + 1 + f.name.size() + (f.world.empty() ? 0 : 1 + f.world.size());
        const std::size_t separator = i < count ? kSeparator.size() : 0;
        // Room must stay for "..." unless this is the last (outermost) frame.
        const std::size_t reserve = i > 1 ? kCut.size() : 0;
        if (segment + separator + reserve > pos) {
            cut = true;
            break;
        }
        if (separator != 0) put(kSeparator);
        if (!f.world.empty()) {
            put(f.world);
            put("@");
        }
        put(f.name);
        put(":");
        put(f.category);
    }
    if (cut && pos >= kCut.size()) put(kCut);
    const std::size_t size = out.size() - pos;
    std::memmove(out.data(), out.data() + pos, size);
    return size;
}

void WritePath(const Stack& s, const std::size_t index, const std::string_view text) noexcept {
    if (!Taken(kPathCategory)) return;
    char buffer[kPathBytes];
    const std::size_t size = BuildPath(s, index + 1, buffer);
    const Frame& f = s.frames[index];
    const Field fields[] = {{"path", std::string_view(buffer, size)}, {"span_id", f.id}};
    Logging::Write(Logging::Entry{.level = Level::Trace, .category = kPathCategory, .text = text, .fields = fields, .world = f.world});
}

void SetError(Frame& f, const std::string_view text) noexcept {
    f.hasError = true;
    f.errorSize = text.size() < kErrorBytes ? text.size() : kErrorBytes;
    std::memcpy(f.error, text.data(), f.errorSize);
}

void WriteSpan(const Frame& f, const bool unbalanced) noexcept {
    char category[kSpanPrefix.size() + kMaxCategory];
    const std::string_view spanCategory = SpanCategory(f.category, category);
    // Asked again at the end: the Logger may have been uninstalled, or the sink changed, while the span was open.
    if (!Taken(spanCategory)) return;
    const std::int64_t end = Now();
    Field fields[7] = {{"span_id", f.id}, {"parent_id", f.parent}, {"begin_ns", f.beginNs}, {"duration_ns", end - f.beginNs},
                       {"", 0}, {"", 0}, {"", 0}};
    std::size_t count = 4;
    if (f.hasError) fields[count++] = Field{"error", std::string_view(f.error, f.errorSize)};
    if (f.exception) fields[count++] = Field{"exception", true};
    if (unbalanced) fields[count++] = Field{"unbalanced", true};
    Logging::Write(Logging::Entry{.level = Level::Trace,
                                  .category = spanCategory,
                                  .text = f.name,
                                  .fields = std::span<const Field>(fields, count),
                                  .world = f.world});
}

// ---- the receiver

Base::TraceToken OnBegin(const std::string_view category, const std::string_view name, const std::string_view world) noexcept {
    if (!installed.load(std::memory_order_acquire)) return {};
    const std::uint32_t current = generation.load(std::memory_order_acquire);
    Stack& s = CurrentStack(current);
    const std::uint64_t id = nextId.fetch_add(1, std::memory_order_relaxed);
    // A new child: an exception that unwound an earlier child was caught, so the parent's next one is its own.
    if (s.depth > 0 && s.depth <= kMaxDepth) s.frames[s.depth - 1].childUnwound = false;
    if (s.depth < kMaxDepth) {
        Frame& f = s.frames[s.depth];
        f.id = id;
        f.parent = s.depth > 0 ? s.frames[s.depth - 1].id : 0;
        f.category = category;
        f.name = name;
        f.world = world;
        f.hasError = false;
        f.exception = false;
        f.childUnwound = false;
        f.errorSize = 0;
        char buffer[kSpanPrefix.size() + kMaxCategory];
        f.timed = Taken(SpanCategory(category, buffer));
        f.beginNs = f.timed ? Now() : 0;
    }
    ++s.depth;
    return Base::TraceToken{.id = id, .generation = current, .depth = static_cast<std::uint32_t>(s.depth)};
}

// The stack and the frame of token, or nullptr when the token is stale or its frame was not stored.
Frame* Resolve(const Base::TraceToken token, Stack*& out, std::size_t& index) noexcept {
    if (token.id == 0 || !installed.load(std::memory_order_acquire)) return nullptr;
    const std::uint32_t current = generation.load(std::memory_order_acquire);
    if (token.generation != current) return nullptr;
    Stack& s = CurrentStack(current);
    out = &s;
    index = Find(s, token.id);
    return index < kMaxDepth ? &s.frames[index] : nullptr;
}

void OnEnd(const Base::TraceToken token, const bool endedByException) noexcept {
    Stack* s = nullptr;
    std::size_t index = 0;
    Frame* f = Resolve(token, s, index);
    if (f == nullptr) {
        // A frame past kMaxDepth: only the count goes down.
        if (s != nullptr && token.depth > kMaxDepth && s->depth == token.depth) --s->depth;
        return;
    }
    const bool unbalanced = index + 1 != StoredCount(*s) || s->depth > kMaxDepth;
    if (endedByException) {
        f->exception = true;
        // The first span the exception unwinds is where it showed (declaration D4 (i)).
        if (!f->childUnwound) {
            if (!f->hasError) SetError(*f, kUnwoundText);
            WritePath(*s, index, kUnwoundText);
        }
        if (index > 0) s->frames[index - 1].childUnwound = true;
    }
    if (f->timed) WriteSpan(*f, unbalanced);
    // Out of the stack; the frames above (only when unbalanced) move down one.
    const std::size_t stored = StoredCount(*s);
    for (std::size_t i = index; i + 1 < stored; ++i) {
        Frame& to = s->frames[i];
        const Frame& from = s->frames[i + 1];
        to.id = from.id;
        to.parent = from.parent;
        to.category = from.category;
        to.name = from.name;
        to.world = from.world;
        to.beginNs = from.beginNs;
        to.timed = from.timed;
        to.hasError = from.hasError;
        to.exception = from.exception;
        to.childUnwound = from.childUnwound;
        to.errorSize = from.errorSize;
        std::memcpy(to.error, from.error, from.errorSize);
    }
    --s->depth;
}

void Mark(Stack& s, const std::size_t index, const std::string_view text, const bool origin) noexcept {
    SetError(s.frames[index], text);
    // Marked after a child's exception was caught: an exception from here on is this span's own.
    s.frames[index].childUnwound = false;
    if (origin) WritePath(s, index, text);
}

void OnError(const Base::TraceToken token, const std::string_view text, const bool origin) noexcept {
    Stack* s = nullptr;
    std::size_t index = 0;
    if (Resolve(token, s, index) != nullptr) Mark(*s, index, text, origin);
}

void OnMarkCurrent(const std::string_view text, const bool origin) noexcept {
    if (!installed.load(std::memory_order_acquire)) return;
    Stack& s = CurrentStack(generation.load(std::memory_order_acquire));
    if (s.depth == 0 || s.depth > kMaxDepth) return;
    Mark(s, s.depth - 1, text, origin);
}

std::size_t OnPath(const std::span<char> buffer) noexcept {
    if (!installed.load(std::memory_order_acquire)) return 0;
    const Stack& s = CurrentStack(generation.load(std::memory_order_acquire));
    return BuildPath(s, StoredCount(s), buffer);
}

constexpr Base::TraceReceiver kReceiver{
    .begin = &OnBegin, .end = &OnEnd, .error = &OnError, .markCurrent = &OnMarkCurrent, .path = &OnPath};

} // namespace

TraceHandle Install(const TraceOptions options) {
    GYO_ASSERT(!installed.load(std::memory_order_acquire) && "Trace::Install: one per process");
    clock = options.monoNs;
    generation.fetch_add(1, std::memory_order_acq_rel);
    installed.store(true, std::memory_order_release);
    previousReceiver = Base::SetTraceReceiver(&kReceiver);
    return TraceHandle(true);
}

TraceHandle::TraceHandle(TraceHandle&& other) noexcept : installed_(other.installed_) {
    other.installed_ = false;
}

TraceHandle& TraceHandle::operator=(TraceHandle&& other) noexcept {
    if (this != &other) {
        Close();
        installed_ = other.installed_;
        other.installed_ = false;
    }
    return *this;
}

TraceHandle::~TraceHandle() {
    Close();
}

void TraceHandle::Close() noexcept {
    if (!installed_) return;
    installed_ = false;
    Base::SetTraceReceiver(previousReceiver);
    previousReceiver = nullptr;
    installed.store(false, std::memory_order_release);
}

} // namespace GYO::Engine::Trace
