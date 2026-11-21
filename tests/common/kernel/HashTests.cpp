// The World summary with a hash function per type (v8-arch-gyo2-authority-rollback
// step 1; declaration sections 3.2, 3.3 and the Kernel cases of 9.3). Each CHECK
// carries a tag in brackets ("[ks-hash-...]"): the step's mutants expect that tag
// (docs/architecture/plans/kernel/scripts/mutations.json, ids ks-hash-).

#include "WorldTestSupport.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace WorldTest;

template <class T>
std::vector<std::byte> HashOf(const T& value) {
    Hasher h;
    HashValue(h, value);
    return {h.Data().begin(), h.Data().end()};
}

std::vector<std::byte> Le64(std::uint64_t value) {
    std::vector<std::byte> out;
    for (int shift = 0; shift < 64; shift += 8) out.push_back(static_cast<std::byte>((value >> shift) & 0xFFU));
    return out;
}

enum class Kind : std::uint8_t { A = 3 };

// Three bytes of padding after a: hashed field by field, the padding is not read.
struct Padded {
    std::uint8_t a;
    std::uint32_t b;
};
void HashValue(Hasher& h, const Padded& v) {
    HashValue(h, v.a);
    HashValue(h, v.b);
}

// The checker's example (declaration 3.2): unique object representations, yet
// an empty optional leaves bytes behind.
struct Maybe {
    std::optional<std::uint8_t> a;
    std::uint16_t b;
};
void HashValue(Hasher& h, const Maybe& v) {
    HashValue(h, v.a);
    HashValue(h, v.b);
}

// Holds the heap: the contents are hashed, never the address.
struct Path {
    std::vector<std::int32_t> points;
    std::string name;
};
void HashValue(Hasher& h, const Path& v) {
    HashValue(h, v.points);
    HashValue(h, v.name);
}

// The author normalizes +0 and -0 in the own function.
struct Speed {
    float v;
};
void HashValue(Hasher& h, const Speed& v) { HashValue(h, v.v + 0.0f); }

// The author picks the bytes as they are.
struct Raw {
    std::uint32_t a;
    std::uint32_t b;
};
void HashValue(Hasher& h, const Raw& v) { HashAsBytes(h, v); }

// No hash function: a component and a resource.
struct Unhashed {
    std::int32_t v;
};
struct UnhashedSetting {
    std::int32_t v;
};

static_assert(Hashable<std::int32_t> && Hashable<Kind> && Hashable<double> && Hashable<std::string>);
static_assert(Hashable<std::vector<std::optional<std::pair<std::int16_t, float>>>>);
static_assert(Hashable<Padded> && Hashable<Raw> && Hashable<RangeAllotter>);
static_assert(!Hashable<Unhashed> && !Hashable<std::vector<Unhashed>> && !Hashable<long double>);

// A value built in memory that held fill before (what a stack slot may hold).
template <class T, class... Args>
T BuiltOver(unsigned char fill, Args&&... args) {
    alignas(T) unsigned char raw[sizeof(T)];
    std::memset(raw, fill, sizeof raw);
    T* made = new (raw) T{std::forward<Args>(args)...};
    T copy = *made;
    made->~T();
    return copy;
}

Fingerprint PaddedSummary(unsigned char fill) {
    auto rig = MakeRig([](Registrar& r) {
        r.Component<Padded>({.name = "Padded"});
        r.Schedule(TickSchedule());
    });
    rig->world().Insert(1, BuiltOver<Padded>(fill, std::uint8_t{1}, std::uint32_t{2}));
    return SummaryOf(rig->world());
}

Fingerprint PathSummary(std::vector<std::int32_t> points) {
    auto rig = MakeRig([](Registrar& r) {
        r.Component<Path>({.name = "Path"});
        r.Schedule(TickSchedule());
    });
    rig->world().Insert(1, Path{std::move(points), "route"});
    return SummaryOf(rig->world());
}

void UnhashedData(Registrar& r) {
    r.Component<Unhashed>({.name = "Unhashed"});
    r.Component<Tag>({.name = "Tag"});
    r.Resource<UnhashedSetting>({.name = "UnhashedSetting"});
    r.Schedule(TickSchedule());
}

} // namespace

TEST_CASE("D-9 hash: an integer hashes as 64 bits little-endian, whatever its width") {
    CHECK_MESSAGE(HashOf(std::uint16_t{258}) == Le64(258), "[ks-hash-int-le64] a uint16 is not 8 bytes little-endian");
    CHECK_MESSAGE(HashOf(std::int32_t{-1}) == HashOf(std::int64_t{-1}), "[ks-hash-int-width] -1 hashes by its width");
    CHECK_MESSAGE(HashOf(static_cast<long>(-5)) == HashOf(static_cast<long long>(-5)), "[ks-hash-int-width] long and long long differ");
    CHECK_MESSAGE(HashOf(Kind::A) == HashOf(std::uint8_t{3}), "[ks-hash-enum] an enum is not its underlying value");
    CHECK_MESSAGE(HashOf(true) == Le64(1), "[ks-hash-bool] true is not 1");
    CHECK_MESSAGE(HashOf(static_cast<char>(0xFF)) == HashOf(static_cast<unsigned char>(0xFF)),
                  "[ks-hash-char] a char hashes by the platform's signedness");
}

TEST_CASE("D-9 hash: a float hashes bit by bit; the author normalizes +0 and -0 in the own function") {
    CHECK_MESSAGE(HashOf(-0.0f) != HashOf(0.0f), "[ks-hash-float-bits] -0.0f and +0.0f hash the same");
    CHECK_MESSAGE(HashOf(-0.0) != HashOf(0.0), "[ks-hash-float-bits] -0.0 and +0.0 hash the same");
    CHECK_MESSAGE(HashOf(1.5f) != HashOf(2.5f), "[ks-hash-float-bits] two floats hash the same");
    CHECK_MESSAGE(HashOf(Speed{-0.0f}) == HashOf(Speed{0.0f}), "[ks-hash-float-normalized] the author's normalization is lost");
}

TEST_CASE("D-9 hash: containers hash their contents") {
    CHECK_MESSAGE(HashOf(std::vector<std::int32_t>{1, 2}) != HashOf(std::vector<std::int32_t>{1, 3}),
                  "[ks-hash-vector-contents] different contents hash the same");
    // Without each vector's size, {{1, 2}, {}} and {{1}, {2}} are the same elements in a row.
    CHECK_MESSAGE(HashOf(std::vector<std::vector<std::int32_t>>{{1, 2}, {}}) !=
                      HashOf(std::vector<std::vector<std::int32_t>>{{1}, {2}}),
                  "[ks-hash-vector-size] where one vector ends and the next begins is lost");
    CHECK_MESSAGE(HashOf(std::string("ab")) != HashOf(std::string("ac")), "[ks-hash-string] different strings hash the same");
    std::optional<std::uint8_t> left = std::uint8_t{7};
    left.reset();
    CHECK_MESSAGE(HashOf(left) == HashOf(std::optional<std::uint8_t>{}),
                  "[ks-hash-optional-empty] an empty optional hashes what it held before");
    CHECK_MESSAGE(HashOf(std::optional<std::uint8_t>{}) != HashOf(std::optional<std::uint8_t>{std::uint8_t{0}}),
                  "[ks-hash-optional-present] empty and 0 hash the same");
    CHECK_MESSAGE(HashOf(std::vector<std::optional<std::uint8_t>>{std::nullopt, std::uint8_t{0}}) !=
                      HashOf(std::vector<std::optional<std::uint8_t>>{std::uint8_t{0}, std::nullopt}),
                  "[ks-hash-optional-present] which element is empty is lost");
    CHECK_MESSAGE(HashOf(std::pair<std::int32_t, std::int32_t>{1, 2}) != HashOf(std::pair<std::int32_t, std::int32_t>{2, 1}),
                  "[ks-hash-pair] a pair's order is lost");
    std::vector<std::byte> oneTwo = Le64(1);
    const std::vector<std::byte> two = Le64(2);
    oneTwo.insert(oneTwo.end(), two.begin(), two.end());
    CHECK_MESSAGE(HashOf(std::array<std::int32_t, 2>{1, 2}) == oneTwo, "[ks-hash-array] an array is not its elements in order");
    Maybe zeros = BuiltOver<Maybe>(0x00);
    Maybe ones = BuiltOver<Maybe>(0xFF);
    zeros.b = ones.b = 9;
    CHECK_MESSAGE(HashOf(zeros) == HashOf(ones), "[ks-hash-optional-empty] equal values with an empty optional hash differently");
}

TEST_CASE("D-9 hash: HashAsBytes hashes the object bytes, only where the author picks it") {
    const Raw raw{1, 2};
    std::vector<std::byte> bytes(sizeof raw);
    std::memcpy(bytes.data(), &raw, sizeof raw);
    CHECK_MESSAGE(HashOf(raw) == bytes, "[ks-hash-as-bytes] HashAsBytes is not the object bytes");
}

TEST_CASE("D-9 hash: in the World summary, padding is not hashed and heap contents are") {
    CHECK_MESSAGE(PaddedSummary(0x00) == PaddedSummary(0xFF), "[ks-hash-summary-padding] padding reached the summary");
    CHECK_MESSAGE(PathSummary({1, 2, 3}) != PathSummary({1, 2, 4}), "[ks-hash-summary-heap] different heap contents, one summary");
    CHECK_MESSAGE(PathSummary({1, 2, 3}) == PathSummary({1, 2, 3}), "[ks-hash-summary-heap] the same contents at other addresses differ");
}

TEST_CASE("D-9 hash: a type with no hash function makes Summary an Err while a value of it is in the World") {
    auto rig = MakeRig(&UnhashedData);
    World& world = rig->world();
    world.Insert(1, Tag{1});
    const auto empty = world.Summary();
    CHECK_MESSAGE(static_cast<bool>(empty), "[ks-hash-missing-only-with-value] registered with no value is reported: "
                                                << (empty ? std::string() : empty.error().Text()));
    world.Insert(2, Unhashed{5});
    world.SetResource(UnhashedSetting{6});
    const auto held = world.Summary();
    REQUIRE_MESSAGE(!held, "[ks-hash-missing-reported] a value of a type with no HashValue went into the summary");
    CHECK_MESSAGE(held.error().code == KernelErrorCode::HashMissing, "[ks-hash-missing-code] " << held.error().Text());
    CHECK_MESSAGE(held.error().detail == "component Unhashed, resource UnhashedSetting",
                  "[ks-hash-missing-names] " << held.error().detail);
    CHECK_MESSAGE(held.error().world == "test-world", "[ks-hash-missing-world] " << held.error().Text());
    world.RemoveResource<UnhashedSetting>();
    world.Remove<Unhashed>(2);
    const auto gone = world.Summary();
    CHECK_MESSAGE(static_cast<bool>(gone), "[ks-hash-missing-only-with-value] the values are gone, the Err stays: "
                                               << (gone ? std::string() : gone.error().Text()));
}
