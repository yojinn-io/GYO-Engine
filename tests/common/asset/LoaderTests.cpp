// Calling a loader directly (declaration 5.16, 5.17, section 10 "直接呼叫"; Design Invariant AS-3): this
// executable links GYO::Asset only. The same input gives the same result, also from two threads; a loader
// that declares references gives the same list twice. Choosing a loader by (type, format) is the store's;
// here only the loader's declared formats and the type name trait are checked.
#include <doctest/doctest.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "engine/asset/AssetLoader.hpp"
#include "engine/asset/LoadContext.hpp"
#include "engine/asset/Text.hpp"
#include "engine/asset/TextLoader.hpp"

namespace Asset = GYO::Engine::Asset;
namespace Base = GYO::Engine::Base;

namespace {

std::span<const std::byte> Bytes(const std::string& text) {
    return {reinterpret_cast<const std::byte*>(text.data()), text.size()};
}

// A test type whose content lists what it references: "id:<id>" or "path:<path>", one per line.
struct LinkedText {
    std::vector<Asset::AssetRef<Asset::Text>> references;
    friend bool operator==(const LinkedText&, const LinkedText&) = default;
};

enum class LinkedTextCode : std::uint8_t { BadLine = 1 };
[[maybe_unused]] constexpr const char* ToString(const LinkedTextCode) noexcept { return "BadLine"; }

} // namespace

template <>
struct Asset::AssetTypeName<LinkedText> {
    static constexpr std::string_view value = "linked_text";
};

namespace {

struct LinkedTextLoader {
    using Output = LinkedText;
    using Settings = Asset::TextSettings;
    using Code = LinkedTextCode;
    static constexpr std::string_view kTypeName = "linked_text";
    static constexpr std::array<std::string_view, 1> kFormats{"links"};
    static constexpr std::size_t kDefaultMaxBytes = 1024;

    Base::Result<Settings, Asset::LoaderFailure<Code>> ParseSettings(std::optional<std::string_view>) const {
        return Settings{};
    }

    Base::Result<LinkedText, Asset::LoaderFailure<Code>> Load(std::span<const std::byte> bytes, const Settings&,
                                                             Asset::LoadContext& context) const {
        const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        LinkedText value;
        std::size_t start = 0;
        while (start < text.size()) {
            std::size_t end = text.find('\n', start);
            if (end == std::string_view::npos) end = text.size();
            const std::string_view line = text.substr(start, end - start);
            if (line.starts_with("id:")) {
                value.references.push_back(context.Reference<Asset::Text>(Asset::AssetId(line.substr(3))));
            } else if (line.starts_with("path:")) {
                value.references.push_back(context.ReferencePath<Asset::Text>(line.substr(5)));
            } else {
                return Base::Err(Asset::LoaderFailure<Code>::Make(Code::BadLine, "bad line", std::string(line)));
            }
            start = end + 1;
        }
        return value;
    }
};

// Declares a type name other than its Output's.
struct WrongNameLoader {
    using Output = Asset::Text;
    using Settings = Asset::TextSettings;
    using Code = Asset::TextLoaderCode;
    static constexpr std::string_view kTypeName = "texture";
    [[maybe_unused]] static constexpr std::array<std::string_view, 1> kFormats{"txt"};
    [[maybe_unused]] static constexpr std::size_t kDefaultMaxBytes = 1024;
    Base::Result<Settings, Asset::LoaderFailure<Code>> ParseSettings(std::optional<std::string_view>) const {
        return Settings{};
    }
    Base::Result<Asset::Text, Asset::LoaderFailure<Code>> Load(std::span<const std::byte>, const Settings&,
                                                              Asset::LoadContext&) const {
        return Asset::Text(std::string());
    }
};

// Takes no format.
struct NoFormatLoader {
    using Output = Asset::Text;
    using Settings = Asset::TextSettings;
    using Code = Asset::TextLoaderCode;
    static constexpr std::string_view kTypeName = "text";
    [[maybe_unused]] static constexpr std::array<std::string_view, 0> kFormats{};
    [[maybe_unused]] static constexpr std::size_t kDefaultMaxBytes = 1024;
    Base::Result<Settings, Asset::LoaderFailure<Code>> ParseSettings(std::optional<std::string_view>) const {
        return Settings{};
    }
    Base::Result<Asset::Text, Asset::LoaderFailure<Code>> Load(std::span<const std::byte>, const Settings&,
                                                              Asset::LoadContext&) const {
        return Asset::Text(std::string());
    }
};

// What one call gave, comparable.
struct Outcome {
    bool loaded = false;
    std::string bytes;
    std::string failure;
    friend bool operator==(const Outcome&, const Outcome&) = default;
};

Outcome LoadText(const std::string& input) {
    const Asset::TextLoader loader;
    Asset::LoadContext context(Asset::AssetId("sample.text"), "docs/sample.txt", Asset::TextLoader::kDefaultMaxBytes);
    auto settings = loader.ParseSettings(std::nullopt);
    REQUIRE(settings);
    auto value = loader.Load(Bytes(input), *settings, context);
    if (value) return Outcome{true, value->Bytes(), {}};
    return Outcome{false, {}, Base::Describe(value.error())};
}

struct Sample {
    const char* name;
    std::string bytes;
    bool utf8;
};

const std::vector<Sample>& Samples() {
    static const std::vector<Sample> samples = {
        {"empty", "", true},
        {"ascii", "{\"hello\": 1}\n", true},
        {"two-byte", "caf\xc3\xa9", true},
        {"three-byte", "\xe6\xbc\xa2\xe5\xad\x97", true},
        {"four-byte", "\xf0\x9f\x98\x80", true},
        {"max-code-point", "\xf4\x8f\xbf\xbf", true},
        {"byte-order-mark", "\xef\xbb\xbfx", true},
        {"nul", std::string("a\0b", 3), true},
        {"lone-continuation", "a\x80", false},
        {"overlong-2", "\xc0\xaf", false},
        {"overlong-3", "\xe0\x80\xaf", false},
        {"overlong-4", "\xf0\x80\x80\xaf", false},
        {"surrogate", "\xed\xa0\x80", false},
        {"above-10ffff", "\xf4\x90\x80\x80", false},
        {"lead-f5", "\xf5\x80\x80\x80", false},
        {"bad-continuation", "\xe3\x81\x41", false},
        {"truncated", "ok\xe3\x81", false},
    };
    return samples;
}

} // namespace

TEST_CASE("loader: the text loader's declaration") {
    static_assert(Asset::AssetLoader<Asset::TextLoader>);
    static_assert(Asset::AssetLoader<LinkedTextLoader>);
    CHECK(Asset::AssetTypeName<Asset::Text>::value == "text");
    CHECK(Asset::TextLoader::kTypeName == "text");
    CHECK(Asset::TextLoader::kFormats == std::array<std::string_view, 2>{"txt", "json"});
    CHECK(Asset::TextLoader::kDefaultMaxBytes == std::size_t{16} * 1024 * 1024);
    CHECK_MESSAGE(!Asset::AssetLoader<WrongNameLoader>,
                  "a loader whose type name differs from its Output's is accepted");
    CHECK_MESSAGE(!Asset::AssetLoader<NoFormatLoader>, "a loader with no format is accepted");
    const Asset::TextLoader loader;
    CHECK(loader.ParseSettings(std::nullopt));
    CHECK(loader.ParseSettings(std::string_view(R"({"anything":1})")));
}

TEST_CASE("loader: the text loader checks UTF-8 and hands the bytes over unchanged") {
    for (const Sample& sample : Samples()) {
        const std::string name = sample.name;
        CAPTURE(name);
        const Outcome outcome = LoadText(sample.bytes);
        CHECK_MESSAGE(outcome.loaded == sample.utf8, "wrong UTF-8 verdict on " << name);
        if (outcome.loaded) {
            CHECK_MESSAGE(outcome.bytes == sample.bytes, "the bytes were changed for " << name);
        } else {
            CHECK(outcome.failure.starts_with("NotUtf8: "));
        }
    }
}

TEST_CASE("loader: the same input twice gives the same result") {
    for (const Sample& sample : Samples()) {
        const std::string name = sample.name;
        CAPTURE(name);
        CHECK(LoadText(sample.bytes) == LoadText(sample.bytes));
    }
}

TEST_CASE("loader: two threads of 1000 calls each give what one thread gives") {
    std::vector<Outcome> expected;
    for (const Sample& sample : Samples()) expected.push_back(LoadText(sample.bytes));
    std::atomic<int> mismatches{0};
    std::atomic<int> calls{0};
    const auto run = [&] {
        const Asset::TextLoader loader;
        for (int call = 0; call < 1000; ++call) {
            const std::size_t index = static_cast<std::size_t>(call) % Samples().size();
            Asset::LoadContext context(Asset::AssetId("sample.text"), "docs/sample.txt",
                                       Asset::TextLoader::kDefaultMaxBytes);
            auto value = loader.Load(Bytes(Samples()[index].bytes), Asset::TextSettings{}, context);
            const Outcome outcome = value ? Outcome{true, value->Bytes(), {}}
                                          : Outcome{false, {}, Base::Describe(value.error())};
            if (!(outcome == expected[index])) ++mismatches;
            ++calls;
        }
    };
    std::thread first(run);
    std::thread second(run);
    first.join();
    second.join();
    CHECK(calls == 2000);
    CHECK(mismatches == 0);
}

TEST_CASE("loader: a loader that declares references gives the same list twice") {
    const std::string input = "id:shared.palette\npath:textures/skin.png\npath:../../../escape.png\nid:shared.palette";
    const LinkedTextLoader loader;
    const auto call = [&] {
        Asset::LoadContext context(Asset::AssetId("hero.links"), "models/hero/hero.links", 1024);
        auto value = loader.Load(Bytes(input), Asset::TextSettings{}, context);
        REQUIRE(value);
        return std::make_pair(*value, context.References());
    };
    const auto [firstValue, firstReferences] = call();
    const auto [secondValue, secondReferences] = call();
    REQUIRE_MESSAGE(firstReferences.size() == 4, "the declared references are not all recorded");
    CHECK(firstReferences == secondReferences);
    CHECK(firstValue == secondValue);

    const Asset::DeclaredReference& byId = firstReferences[0];
    CHECK(byId.kind == Asset::ReferenceKind::Id);
    CHECK(byId.typeName == "text");
    CHECK(byId.id == Asset::AssetId("shared.palette"));
    CHECK_FALSE(byId.failure.has_value());
    CHECK(firstValue.references[0].Id() == Asset::AssetId("shared.palette"));

    const Asset::DeclaredReference& byPath = firstReferences[1];
    CHECK(byPath.kind == Asset::ReferenceKind::Path);
    CHECK(byPath.typeName == "text");
    CHECK(byPath.writtenPath == "textures/skin.png");
    CHECK(byPath.path == "models/hero/textures/skin.png");
    CHECK_FALSE(byPath.failure.has_value());
    // Mapping a path to an id is the store's: the reference has no id yet.
    CHECK(firstValue.references[1].Id().Empty());

    const Asset::DeclaredReference& escaping = firstReferences[2];
    CHECK(escaping.writtenPath == "../../../escape.png");
    CHECK(escaping.path.empty());
    CHECK_MESSAGE(escaping.failure == std::optional(Asset::ReferenceFailureCode::BadReference),
                  "a path above the content root is not BadReference");
}

TEST_CASE("loader: ReferencePath refuses what cannot name a content file") {
    const char* const bad[] = {"../../../a.png", "/abs/a.png", "tex\\a.png", "C:/a.png", "a//b.png", ""};
    for (const char* reference : bad) {
        const std::string written = reference;
        CAPTURE(written);
        Asset::LoadContext context(Asset::AssetId("hero.model"), "models/hero/hero.fbx", 1024);
        const Asset::AssetRef<Asset::Text> ref = context.ReferencePath<Asset::Text>(reference);
        CHECK(ref.Id().Empty());
        REQUIRE(context.References().size() == 1);
        CHECK(context.References()[0].failure == std::optional(Asset::ReferenceFailureCode::BadReference));
        CHECK(context.References()[0].writtenPath == written);
    }
    Asset::LoadContext context(Asset::AssetId("hero.model"), "models/hero/hero.fbx", 1024);
    CHECK(context.Id() == Asset::AssetId("hero.model"));
    CHECK(context.Path() == "models/hero/hero.fbx");
    CHECK(context.MaxBytes() == 1024);
}

namespace {

// A catalog's path index as the store gives one.
class FixedPaths final : public Asset::ContentPathIndex {
public:
    std::span<const Asset::AssetId> IdsAt(std::string_view path) const override {
        if (path == "models/tex/skin.png") return {one_.data(), 1};
        if (path == "shared/dup.png") return {two_.data(), 2};
        return {};
    }

private:
    std::array<Asset::AssetId, 1> one_{Asset::AssetId("skin")};
    std::array<Asset::AssetId, 2> two_{Asset::AssetId("dup.one"), Asset::AssetId("dup.two")};
};

} // namespace

TEST_CASE("loader: with a path index, ReferencePath maps a path to an id or names why not") {
    const FixedPaths paths;
    Asset::LoadContext context(Asset::AssetId("hero.model"), "models/hero.fbx", 1024, &paths);
    const auto listed = context.ReferencePath<Asset::Text>("tex/skin.png");
    const auto unlisted = context.ReferencePath<Asset::Text>("tex/none.png");
    const auto ambiguous = context.ReferencePath<Asset::Text>("../shared/dup.png");
    const auto bad = context.ReferencePath<Asset::Text>("../../x.png");
    CHECK(listed.Id() == Asset::AssetId("skin"));
    REQUIRE(context.References().size() == 4);
    CHECK(context.References()[0].id == Asset::AssetId("skin"));
    CHECK_FALSE(context.References()[0].failure.has_value());
    CHECK(unlisted.Id().Empty());
    CHECK_MESSAGE(context.References()[1].failure == std::optional(Asset::ReferenceFailureCode::UnlistedReference),
                  "a path no entry has is not UnlistedReference");
    CHECK(ambiguous.Id().Empty());
    CHECK_MESSAGE(context.References()[2].failure == std::optional(Asset::ReferenceFailureCode::AmbiguousReference),
                  "a path two entries have is not AmbiguousReference");
    CHECK(bad.Id().Empty());
    CHECK(context.References()[3].failure == std::optional(Asset::ReferenceFailureCode::BadReference));
}
