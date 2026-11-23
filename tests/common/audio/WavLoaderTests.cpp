#include "doctest/doctest.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "engine/asset/AssetSnapshot.hpp"
#include "engine/asset/AssetStore.hpp"
#include "engine/audio/AudioFormat.hpp"
#include "engine/audio/Mixer.hpp"
#include "engine/audio/WavLoader.hpp"

// The "sound"/"wav" Data Contract (audio.zh-Hant.md section 6; audio-clip-loader
// declaration 5.2): every condition of every rule, the order of the rules,
// master's content, determinism (AS-3) and the way through the asset store.

using namespace GYO::Engine;
using Audio::WavLoader;
using Audio::WavLoaderCode;

namespace {

using Bytes = std::vector<std::byte>;

void Put(Bytes& out, const char (&id)[5]) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::byte>(id[i]));
}
void Put16(Bytes& out, const std::uint32_t v) {
    out.push_back(static_cast<std::byte>(v & 0xFF));
    out.push_back(static_cast<std::byte>((v >> 8) & 0xFF));
}
void Put32(Bytes& out, const std::uint32_t v) {
    Put16(out, v & 0xFFFF);
    Put16(out, v >> 16);
}

// One chunk; an odd body gets its pad byte unless pad is false. size, when
// given, is written instead of the body's size.
Bytes Chunk(const char (&id)[5], const Bytes& body, const bool pad = true, const std::uint32_t* size = nullptr) {
    Bytes out;
    Put(out, id);
    Put32(out, size != nullptr ? *size : static_cast<std::uint32_t>(body.size()));
    out.insert(out.end(), body.begin(), body.end());
    if (pad && body.size() % 2 != 0) out.push_back(std::byte{0});
    return out;
}

Bytes Fmt(const std::uint32_t channels = 2, const std::uint32_t rate = 48000, const std::uint32_t tag = 1,
          const std::uint32_t bits = 16, const std::uint32_t blockAlign = 0, const std::size_t size = 16) {
    Bytes body;
    Put16(body, tag);
    Put16(body, channels);
    Put32(body, rate);
    Put32(body, rate * channels * 2);
    Put16(body, blockAlign != 0 ? blockAlign : channels * 2);
    Put16(body, bits);
    body.resize(size, std::byte{0});
    return Chunk("fmt ", body);
}

Bytes Samples(const std::vector<std::int16_t>& samples) {
    Bytes body;
    for (const std::int16_t s : samples) Put16(body, static_cast<std::uint16_t>(s));
    return body;
}

Bytes Data(const std::vector<std::int16_t>& samples) { return Chunk("data", Samples(samples)); }

Bytes Riff(const std::vector<Bytes>& chunks) {
    Bytes out;
    Put(out, "RIFF");
    std::size_t size = 4;
    for (const Bytes& c : chunks) size += c.size();
    Put32(out, static_cast<std::uint32_t>(size));
    Put(out, "WAVE");
    for (const Bytes& c : chunks) out.insert(out.end(), c.begin(), c.end());
    return out;
}

Base::Result<Audio::Clip, Asset::LoaderFailure<WavLoaderCode>> Load(const Bytes& bytes) {
    Asset::LoadContext context(Asset::AssetId("test.sound"), "audio/test.wav", WavLoader::kDefaultMaxBytes);
    return WavLoader{}.Load(std::span<const std::byte>(bytes), Audio::WavSettings{}, context);
}

void Expect(const Bytes& bytes, const WavLoaderCode code) {
    const auto loaded = Load(bytes);
    REQUIRE_FALSE(loaded.has_value());
    CHECK(loaded.error().code == code);
}

const std::vector<std::int16_t> kStereo{100, -100, 200, -200, 300, -300};

Bytes ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    const std::vector<char> chars((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    Bytes out(chars.size());
    if (!chars.empty()) std::memcpy(out.data(), chars.data(), chars.size());
    return out;
}

} // namespace

TEST_CASE("WavLoader reads the canonical 44-byte header") {
    const Bytes bytes = Riff({Fmt(), Data(kStereo)});
    CHECK(bytes.size() == 44 + kStereo.size() * 2);
    const auto loaded = Load(bytes);
    REQUIRE(loaded.has_value());
    const Audio::ClipView& view = loaded->View();
    CHECK(view.channels == 2);
    CHECK(view.frames == 3);
    for (std::size_t i = 0; i < kStereo.size(); ++i) CHECK(view.samples[i] == kStereo[i]);
    const auto mono = Load(Riff({Fmt(1), Data({-32768, 32767, 1})}));
    REQUIRE(mono.has_value());
    CHECK(mono->View().frames == 3);
    CHECK(mono->View().samples[0] == -32768);
    CHECK(mono->View().samples[1] == 32767);
}

TEST_CASE("WavLoader rule 1: NotWave") {
    Expect(Bytes(11, std::byte{0}), WavLoaderCode::NotWave);
    Bytes notRiff = Riff({Fmt(), Data(kStereo)});
    notRiff[0] = std::byte{'X'};
    Expect(notRiff, WavLoaderCode::NotWave);
    Bytes notWave = Riff({Fmt(), Data(kStereo)});
    notWave[8] = std::byte{'X'};
    Expect(notWave, WavLoaderCode::NotWave);
}

TEST_CASE("WavLoader rule 2: Truncated, the pad byte and the short tail") {
    const std::uint32_t tooLong = 100;
    Expect(Riff({Fmt(), Chunk("data", Samples(kStereo), true, &tooLong)}), WavLoaderCode::Truncated);
    // Fewer than 8 bytes left after the last chunk: ignored.
    Bytes tail = Riff({Fmt(), Data(kStereo)});
    tail.insert(tail.end(), {std::byte{'L'}, std::byte{'I'}, std::byte{'S'}});
    CHECK(Load(tail).has_value());
    // An odd chunk before the others is followed by its pad byte.
    CHECK(Load(Riff({Chunk("LIST", Bytes(3, std::byte{7})), Fmt(), Data(kStereo)})).has_value());
    // The last chunk may lack its pad byte.
    CHECK(Load(Riff({Fmt(), Data(kStereo), Chunk("LIST", Bytes(3, std::byte{7}), false)})).has_value());
}

TEST_CASE("WavLoader rule 3: DuplicateChunk and MissingChunk") {
    Expect(Riff({Fmt(), Fmt(), Data(kStereo)}), WavLoaderCode::DuplicateChunk);
    Expect(Riff({Fmt(), Data(kStereo), Data(kStereo)}), WavLoaderCode::DuplicateChunk);
    Expect(Riff({Data(kStereo)}), WavLoaderCode::MissingChunk);
    Expect(Riff({Fmt()}), WavLoaderCode::MissingChunk);
    // Other chunks are skipped; the order of fmt and data does not matter.
    CHECK(Load(Riff({Chunk("LIST", Bytes(4, std::byte{1})), Data(kStereo), Fmt()})).has_value());
}

TEST_CASE("WavLoader rules 4 to 7: BadFormatChunk, NotPcm16, PartialSample") {
    Expect(Riff({Fmt(2, 48000, 1, 16, 0, 14), Data(kStereo)}), WavLoaderCode::BadFormatChunk); // shorter than 16
    Expect(Riff({Fmt(2, 48000, 3, 16), Data(kStereo)}), WavLoaderCode::NotPcm16);              // the format tag
    Expect(Riff({Fmt(2, 48000, 1, 24), Data(kStereo)}), WavLoaderCode::NotPcm16);              // the bits
    Expect(Riff({Fmt(2, 48000, 1, 16, 2), Data(kStereo)}), WavLoaderCode::BadFormatChunk);     // block align
    Expect(Riff({Fmt(1), Chunk("data", Bytes(7, std::byte{1}))}), WavLoaderCode::PartialSample);
    // A longer fmt chunk (an extension) is accepted.
    CHECK(Load(Riff({Fmt(2, 48000, 1, 16, 0, 18), Data(kStereo)})).has_value());
}

TEST_CASE("WavLoader rule 8: what Clip::Make refuses is InvalidClip") {
    Expect(Riff({Fmt(2, 44100), Data(kStereo)}), WavLoaderCode::InvalidClip);           // the rate
    Expect(Riff({Fmt(3, 48000, 1, 16, 6), Data(kStereo)}), WavLoaderCode::InvalidClip); // 3 channels
    Expect(Riff({Fmt(0, 48000, 1, 16, 0), Data(kStereo)}), WavLoaderCode::InvalidClip); // 0 channels
    Expect(Riff({Fmt(), Data({})}), WavLoaderCode::InvalidClip);                        // empty
    Expect(Riff({Fmt(2), Data({1, 2, 3})}), WavLoaderCode::InvalidClip);                // not whole frames
    const auto rate = Load(Riff({Fmt(2, 44100), Data(kStereo)}));
    REQUIRE_FALSE(rate.has_value());
    CHECK(rate.error().detail.find("48000 Hz") != std::string::npos);
}

TEST_CASE("WavLoader checks its rules in their order") {
    Expect(Riff({Fmt(2, 22050, 1, 24), Data(kStereo)}), WavLoaderCode::NotPcm16); // 5 before 8
    Expect(Riff({Fmt(2, 48000, 1, 16, 0, 14)}), WavLoaderCode::MissingChunk);     // 3b before 4
    Expect(Riff({Fmt(3, 48000, 1, 16, 6), Chunk("data", Bytes(7, std::byte{1}))}),
           WavLoaderCode::PartialSample); // 7 before 8
    const std::uint32_t tooLong = 100;
    Expect(Riff({Fmt(), Fmt(), Chunk("data", Samples(kStereo), true, &tooLong)}),
           WavLoaderCode::Truncated); // 2 runs through every chunk before 3
    Expect(Riff({Fmt(), Fmt()}), WavLoaderCode::DuplicateChunk); // 3a before 3b
}

TEST_CASE("WavLoader loads master's content as it was") {
    // player_footstep.wav from origin/master 3b411e21 (assets/object_fps_pvp/audio), the same git object.
    const Bytes bytes = ReadFile(std::filesystem::path(GYO_AUDIO_CONTRACT_DIR) / "player_footstep.wav");
    const auto loaded = Load(bytes);
    REQUIRE(loaded.has_value());
    const Audio::ClipView& view = loaded->View();
    CHECK(view.channels == 2);
    CHECK(std::size_t{view.frames} * 4 == bytes.size() - 44);
    for (std::size_t i = 0; i < std::size_t{view.frames} * 2; ++i) {
        const auto lo = std::to_integer<unsigned>(bytes[44 + i * 2]);
        const auto hi = std::to_integer<unsigned>(bytes[44 + i * 2 + 1]);
        REQUIRE(view.samples[i] == static_cast<std::int16_t>(lo | (hi << 8)));
    }
}

TEST_CASE("WavLoader gives the same clip for the same bytes, on any thread (AS-3)") {
    const Bytes bytes = Riff({Chunk("LIST", Bytes(5, std::byte{3})), Fmt(), Data(kStereo)});
    const auto first = Load(bytes);
    REQUIRE(first.has_value());
    const auto same = [&](const Audio::Clip& clip) {
        const Audio::ClipView& a = clip.View();
        const Audio::ClipView& b = first->View();
        if (a.channels != b.channels || a.frames != b.frames) return false;
        for (std::size_t i = 0; i < std::size_t{a.frames} * a.channels; ++i) {
            if (a.samples[i] != b.samples[i]) return false;
        }
        return true;
    };
    CHECK(same(*Load(bytes)));
    int mismatches[2] = {0, 0};
    std::thread threads[2];
    for (int t = 0; t < 2; ++t) {
        threads[t] = std::thread([&, t] {
            for (int i = 0; i < 1000; ++i) {
                const auto loaded = Load(bytes);
                if (!loaded || !same(*loaded)) ++mismatches[t];
            }
        });
    }
    for (std::thread& thread : threads) thread.join();
    CHECK(mismatches[0] == 0);
    CHECK(mismatches[1] == 0);
}

namespace {

// Runs each job at once; a job submitted from a job waits in the queue.
class InlineExecutor final : public Asset::AssetExecutor {
public:
    void Submit(Job job) override {
        queue_.push_back(std::move(job));
        if (running_) return;
        running_ = true;
        while (!queue_.empty()) {
            Job next = std::move(queue_.front());
            queue_.pop_front();
            next();
        }
        running_ = false;
    }

private:
    std::deque<Job> queue_;
    bool running_ = false;
};

// A content root in the temporary directory holding audio/shot.wav.
struct Content final {
    std::filesystem::path root;
    explicit Content(const Bytes& wav) {
        root = std::filesystem::temp_directory_path() /
               ("gyo_audio_wav_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        std::filesystem::create_directories(root / "audio");
        std::ofstream out(root / "audio" / "shot.wav", std::ios::binary);
        out.write(reinterpret_cast<const char*>(wav.data()), static_cast<std::streamsize>(wav.size()));
    }
    Content(const Content&) = delete;
    Content& operator=(const Content&) = delete;
    ~Content() {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
};

std::vector<Asset::CatalogEntry> ShotCatalog() {
    Asset::CatalogEntry entry;
    entry.id = Asset::AssetId("game.audio.shot");
    entry.type = "sound";
    entry.path = "audio/shot.wav";
    return {entry};
}

} // namespace

TEST_CASE("WavLoader through the asset store: loaded, shared, played; and the size limit") {
    std::vector<std::int16_t> samples;
    for (int i = 0; i < 96; ++i) samples.push_back(static_cast<std::int16_t>(i % 2 == 0 ? 1000 : -1000));
    const Bytes wav = Riff({Fmt(), Data(samples)});
    const Content content(wav);
    const auto entries = ShotCatalog();
    InlineExecutor executor;

    SUBCASE("loaded and played without a copy") {
        Asset::AssetStoreOptions options;
        options.contentRoot = content.root.string();
        Asset::AssetStore store(entries, options, &executor);
        REQUIRE(store.Loaders().Register<WavLoader>().has_value());
        const auto handle = store.Request<Audio::Clip>(Asset::AssetId("game.audio.shot"));
        const std::shared_ptr<const Audio::Clip> value = store.Read(handle);
        REQUIRE(value != nullptr);
        const Audio::Clip copy = *value;
        CHECK(copy.View().samples.data() == value->View().samples.data());

        Audio::MixerConfig config;
        config.scheduleDelay = Time::Duration::zero();
        auto mixer = Audio::Mixer::Create(config);
        REQUIRE(mixer.has_value());
        auto submitter = mixer->CreateSubmitter();
        REQUIRE(submitter.has_value());
        const Time::TimePoint anchor =
            Time::TimePoint{} + std::chrono::duration_cast<Time::Duration>(std::chrono::seconds(1));
        REQUIRE(submitter->Play({copy, anchor}));
        std::vector<std::int16_t> out(std::size_t{48} * Audio::kChannels);
        mixer->Render(Base::Span<std::int16_t>(out.data(), out.size()), anchor);
        for (std::size_t i = 0; i < out.size(); ++i) CHECK(out[i] == samples[i]);
    }

    SUBCASE("a file above the limit is TooLarge and the loader is not called") {
        Asset::AssetStoreOptions options;
        options.contentRoot = content.root.string();
        options.byteLimits.push_back(Asset::AssetByteLimit{"sound", "wav", wav.size() - 1});
        Asset::AssetStore store(entries, options, &executor);
        REQUIRE(store.Loaders().Register<WavLoader>().has_value());
        const auto handle = store.Request<Audio::Clip>(Asset::AssetId("game.audio.shot"));
        CHECK(store.Read(handle) == nullptr);
        const Asset::AssetSnapshot snapshot = store.Snapshot();
        const Asset::AssetRecord* const record = snapshot.Find(Asset::AssetId("game.audio.shot"));
        REQUIRE(record != nullptr);
        REQUIRE(record->failure.has_value());
        CHECK(record->failure->code == Asset::AssetLoadFailureCode::TooLarge);
    }
}
