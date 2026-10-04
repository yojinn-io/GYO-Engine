// Product-owned authority digest runner. Every predeclared scenario drives the
// real authority (PvpMatch, MatchRuntimeHost) and Client prediction with
// scripted input, encodes each Tick's authoritative state as little-endian
// bytes and chains them with SHA-256. Two builds that produce the same digest
// for every scenario resolve authority identically; a differing scenario names
// its first differing record (--dump). Scenarios whose every yaw and pitch is
// exactly zero never reach a non-trivial sin/cos (C Annex F: sin(0) = 0,
// cos(0) = 1), so their digests are the cross-platform golden subset; the
// runner refuses a non-zero angle inside that subset. No wire encoding is
// exercised: this proves authority behaviour, not bytes.
#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"
#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"
#include "RetroFPS/Pvp/PvpMatch.hpp"
#include "engine/base/Sha256.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <numbers>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace fps::pvp;

constexpr int DigestVersion = 1;
// Field list of digest version 1, in encoding order. A later version may add
// fields; comparisons "without new fields" select exactly this list.
constexpr std::string_view DigestFields =
    "match_tick: tick\n"
    "player: playerId position.x position.y position.z yaw pitch lastResolvedCommand movementEpoch "
    "contiguousPendingCommands verticalVelocity grounded lifeGeneration lifeState lifeStateTick respawnTick "
    "connectionQualityFailures (movementSlack* excluded: host timing observation)\n"
    "combat: playerId hp nextAllowedShotTick lifeGeneration magazineAmmo reloadActionId reloadStartTick "
    "reloadEndTick lastShotActionId lastShotTick\n"
    "decision: owner actionId resolvedTick accepted rejection hitKind targetId damage kind lifeGeneration "
    "targetLifeGeneration\n"
    "quality: playerId resolved substituted resets\n"
    "prediction: predictedPosition.xyz renderPosition.xyz correctionOffset.xyz lastResolvedCommand latestCommand "
    "authorityTick pendingCommands active frozen movementEpoch serverPendingCommands previousCommand "
    "currentCommand interpolationAlpha verticalVelocity grounded lifeGeneration lifeState phaseTracking "
    "phaseCorrections phaseLateCorrections\n"
    "control: requestId playerId kind accepted error\n"
    "eviction: playerId reason referenceAgeMillis substitutedPermille movementResets\n";

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

// One record: little-endian bytes for the digest and named text for --dump.
class Record final {
public:
    explicit Record(std::string_view kind) { Text("kind", kind); }
    void U64(std::string_view name, std::uint64_t value) {
        for (int shift = 0; shift < 64; shift += 8) bytes_.push_back(static_cast<char>((value >> shift) & 0xFF));
        Field(name, std::to_string(value));
    }
    void I64(std::string_view name, std::int64_t value) { U64(name, static_cast<std::uint64_t>(value)); }
    void F32(std::string_view name, float value) {
        const auto bits = std::bit_cast<std::uint32_t>(value);
        for (int shift = 0; shift < 32; shift += 8) bytes_.push_back(static_cast<char>((bits >> shift) & 0xFF));
        std::ostringstream text;
        text << std::hexfloat << value;
        Field(name, text.str());
    }
    void F64(std::string_view name, double value) {
        const auto bits = std::bit_cast<std::uint64_t>(value);
        for (int shift = 0; shift < 64; shift += 8) bytes_.push_back(static_cast<char>((bits >> shift) & 0xFF));
        std::ostringstream text;
        text << std::hexfloat << value;
        Field(name, text.str());
    }
    void Bool(std::string_view name, bool value) { U64(name, value ? 1 : 0); }
    void Text(std::string_view name, std::string_view value) {
        U64Raw(value.size());
        bytes_.append(value);
        Field(name, std::string(value));
    }
    void Vec(std::string_view name, Engine::Math::Vec3 value) {
        F32(std::string(name) + ".x", value.x);
        F32(std::string(name) + ".y", value.y);
        F32(std::string(name) + ".z", value.z);
    }
    [[nodiscard]] const std::string& Bytes() const noexcept { return bytes_; }
    [[nodiscard]] const std::vector<std::string>& Lines() const noexcept { return lines_; }

private:
    void U64Raw(std::uint64_t value) {
        for (int shift = 0; shift < 64; shift += 8) bytes_.push_back(static_cast<char>((value >> shift) & 0xFF));
    }
    void Field(std::string_view name, std::string value) { lines_.push_back(std::string(name) + '=' + std::move(value)); }
    std::string bytes_;
    std::vector<std::string> lines_;
};

class Digest final {
public:
    Digest(bool golden, bool keepTicks, std::ostream* dump) : golden_(golden), keepTicks_(keepTicks), dump_(dump) {}
    void Add(const Record& record) {
        const std::string input = chain_ + record.Bytes();
        chain_ = Engine::Base::Sha256(std::as_bytes(std::span{input.data(), input.size()}));
        if (keepTicks_) prefixes_.push_back(chain_.substr(0, 16));
        if (dump_) {
            *dump_ << "#" << count_;
            for (const auto& line : record.Lines()) *dump_ << ' ' << line;
            *dump_ << '\n';
        }
        ++count_;
    }
    // Inside the golden subset every angle must be exactly zero.
    void Angle(float value, std::string_view what) {
        if (golden_) Require(value == 0.0F, "golden scenario used a non-zero " + std::string(what));
    }
    [[nodiscard]] const std::string& Value() const noexcept { return chain_; }
    [[nodiscard]] std::uint64_t Count() const noexcept { return count_; }
    [[nodiscard]] const std::vector<std::string>& Prefixes() const noexcept { return prefixes_; }

private:
    bool golden_, keepTicks_;
    std::ostream* dump_;
    std::string chain_ = std::string(64, '0');
    std::uint64_t count_{};
    std::vector<std::string> prefixes_;
};

void EncodePlayer(Record& r, const PlayerState& p) {
    r.U64("player", p.playerId);
    r.Vec("position", p.position);
    r.F32("yaw", p.yaw);
    r.F32("pitch", p.pitch);
    r.U64("lastResolvedCommand", p.lastResolvedCommand);
    r.U64("movementEpoch", p.movementEpoch);
    r.U64("contiguousPendingCommands", p.contiguousPendingCommands);
    r.F32("verticalVelocity", p.verticalVelocity);
    r.Bool("grounded", p.grounded);
    r.U64("lifeGeneration", p.lifeGeneration);
    r.U64("lifeState", static_cast<std::uint64_t>(p.lifeState));
    r.U64("lifeStateTick", p.lifeStateTick);
    r.U64("respawnTick", p.respawnTick);
    r.U64("connectionQualityFailures", p.connectionQualityFailures);
}

void EncodeCombat(Record& r, const CombatState& c) {
    r.U64("combat", c.playerId);
    r.U64("hp", c.hp);
    r.U64("nextAllowedShotTick", c.nextAllowedShotTick);
    r.U64("combatLife", c.lifeGeneration);
    r.U64("magazineAmmo", c.magazineAmmo);
    r.U64("reloadActionId", c.reloadActionId);
    r.U64("reloadStartTick", c.reloadStartTick);
    r.U64("reloadEndTick", c.reloadEndTick);
    r.U64("lastShotActionId", c.lastShotActionId);
    r.U64("lastShotTick", c.lastShotTick);
}

void EncodeDecision(Record& r, PlayerId owner, const ShotDecision& d) {
    r.U64("decisionOwner", owner);
    r.U64("actionId", d.actionId);
    r.U64("resolvedTick", d.resolvedTick);
    r.Bool("accepted", d.accepted);
    r.U64("rejection", static_cast<std::uint64_t>(d.rejection));
    r.U64("hitKind", static_cast<std::uint64_t>(d.hitKind));
    r.U64("targetId", d.targetId);
    r.U64("damage", d.damage);
    r.U64("actionKind", static_cast<std::uint64_t>(d.kind));
    r.U64("decisionLife", d.lifeGeneration);
    r.U64("targetLife", d.targetLifeGeneration);
}

void EncodeSnapshot(Record& r, const WorldSnapshot& snapshot) {
    r.U64("tick", snapshot.tick);
    r.U64("players", snapshot.players.size());
    for (const auto& player : snapshot.players) EncodePlayer(r, player);
    r.U64("combats", snapshot.combat.size());
    for (const auto& combat : snapshot.combat) EncodeCombat(r, combat);
}

// Arenas -------------------------------------------------------------------

Arena Load(const char* path) {
    std::string error;
    auto arena = Arena::Load(path, error);
    Require(arena.has_value(), std::string("Cannot load arena ") + path + ": " + error);
    return *arena;
}

Arena Boxed(std::string id, float width, float depth) {
    Arena arena;
    arena.id = std::move(id);
    arena.width = width;
    arena.depth = depth;
    arena.walls = {{{-1, 0, -1}, {0, 3, depth + 1}}, {{width, 0, -1}, {width + 1, 3, depth + 1}},
                   {{0, 0, -1}, {width, 3, 0}}, {{0, 0, depth}, {width, 3, depth + 1}}};
    return arena;
}

Arena NamedArena(std::string_view name) {
    if (name == "contract") return Load(PVP_DIGEST_CONTRACT_ARENA);
    if (name == "product") return Load(PVP_DIGEST_PRODUCT_ARENA);
    if (name == "pillar") {
        auto arena = Boxed("digest_pillar", 12, 12);
        arena.walls.push_back({{5, 0, 5}, {7, 3, 7}});
        arena.spawns = {{{6, 0, 2}, 0}, {{6, 0, 10}, 0}};
        return arena;
    }
    if (name == "corridor") {
        auto arena = Boxed("digest_corridor", 6, 20);
        arena.spawns = {{{3, 0, 2}, 0}, {{3, 0, 12}, 0}};
        return arena;
    }
    if (name == "corner") {
        auto arena = Boxed("digest_corner", 12, 12);
        arena.walls.push_back({{4, 0, 6}, {10, 3, 7}});
        arena.walls.push_back({{9, 0, 2}, {10, 3, 6}});
        arena.spawns = {{{6, 0, 3}, 0}, {{2, 0, 10}, 0}};
        return arena;
    }
    throw std::runtime_error("Unknown arena " + std::string(name));
}

// Match driver ---------------------------------------------------------------

constexpr double TickSeconds = MovementTickSeconds;

struct Intent final {
    float forward{}, right{}, yaw{}, pitch{};
    bool jump{};
    bool send{true};
};

// Drives a PvpMatch like a Client would: one command per Tick in the current
// epoch and life, shots with contiguous IDs, every decision acknowledged once.
class Duel final {
public:
    Duel(Arena arena, Digest& digest, std::vector<PlayerId> ids) : match_(std::move(arena)), digest_(digest) {
        std::string error;
        for (const auto id : ids) {
            Require(match_.Join(id, error), "join: " + error);
            drivers_[id] = {};
        }
    }
    PvpMatch& Match() noexcept { return match_; }
    const PlayerState* Find(PlayerId id) const {
        for (const auto& p : snapshot_.players) if (p.playerId == id) return &p;
        return nullptr;
    }
    const PlayerState& State(PlayerId id) const {
        const auto* found = Find(id);
        Require(found != nullptr, "missing player");
        return *found;
    }
    // Leave and join again as a new participant: the Client starts over.
    void Rejoin(PlayerId id) {
        Require(match_.Leave(id), "leave");
        std::string error;
        Require(match_.Join(id, error), "rejoin: " + error);
        drivers_[id] = {};
    }
    void Refresh() { snapshot_ = match_.Snapshot(); }
    void Move(PlayerId id, const Intent& intent) {
        Refresh();
        const auto* found = Find(id);
        if (!found || found->lifeState == LifeState::Dead || !intent.send) return;
        const auto& state = *found;
        auto& driver = drivers_[id];
        if (driver.epoch != state.movementEpoch || driver.life != state.lifeGeneration) {
            driver.epoch = state.movementEpoch;
            driver.life = state.lifeGeneration;
            driver.next = state.lastResolvedCommand + 1;
        }
        digest_.Angle(intent.yaw, "movement yaw");
        digest_.Angle(intent.pitch, "movement pitch");
        PlayerInput input{id, {{driver.next++, intent.forward, intent.right, intent.yaw, intent.pitch, intent.jump}},
            state.movementEpoch, state.lifeGeneration, snapshot_.tick};
        static_cast<void>(match_.SubmitInput(input));
    }
    // Pre-sends `count` future commands (window up to MaxPendingCommands each).
    void Flood(PlayerId id, unsigned count) {
        Refresh();
        const auto& state = State(id);
        auto& driver = drivers_[id];
        if (driver.epoch != state.movementEpoch || driver.life != state.lifeGeneration) {
            driver.epoch = state.movementEpoch;
            driver.life = state.lifeGeneration;
            driver.next = state.lastResolvedCommand + 1;
        }
        while (count) {
            PlayerInput input{id, {}, state.movementEpoch, state.lifeGeneration, snapshot_.tick};
            for (unsigned n = 0; n < MaxPendingCommands && count; ++n, --count)
                input.commands.push_back({driver.next++, 1, 0, 0, 0, false});
            static_cast<void>(match_.SubmitInput(input));
        }
    }
    void Act(PlayerId id, ActionKind kind, float yaw = 0, float pitch = 0, std::optional<std::uint64_t> life = {}) {
        Refresh();
        if (kind == ActionKind::Shot) {
            digest_.Angle(yaw, "shot yaw");
            digest_.Angle(pitch, "shot pitch");
        }
        auto& driver = drivers_[id];
        const ShotRequest request{++driver.action, snapshot_.tick, yaw, pitch, kind,
            life.value_or(State(id).lifeGeneration)};
        static_cast<void>(match_.SubmitActions({id, {request}}));
    }
    // Aim from one player's eye at the other's body centre.
    std::pair<float, float> Aim(PlayerId from, PlayerId to) {
        Refresh();
        const auto a = State(from).position, b = State(to).position;
        const float yaw = std::atan2(b.x - a.x, b.z - a.z);
        const float eye = a.y + match_.GetArena().eyeHeight, centre = b.y + match_.GetArena().bodyHeight / 2;
        const float pitch = std::atan2(eye - centre, std::hypot(b.x - a.x, b.z - a.z));
        return {yaw, pitch};
    }
    void Tick() {
        const auto now = match_.TickCount() + 1;
        // Trusted host metadata: a reference's age is its Tick distance.
        match_.Tick({now, TickSeconds}, [now](std::uint64_t observed) -> std::optional<std::chrono::nanoseconds> {
            return std::chrono::nanoseconds(static_cast<std::int64_t>((now - observed) * 1'000'000'000.0 / AuthorityTickRate));
        });
        Refresh();
        Record record("match");
        EncodeSnapshot(record, snapshot_);
        for (auto& [id, driver] : drivers_) {
            if (const auto results = match_.GetActionResults(id)) {
                ActionId through = results->retiredThrough;
                for (const auto& decision : results->decisions) {
                    if (driver.reported.insert(decision.actionId).second) EncodeDecision(record, id, decision);
                    through = std::max(through, decision.actionId);
                }
                if (through > results->retiredThrough) static_cast<void>(match_.AcknowledgeActions(id, through));
            }
            if (const auto quality = match_.GetMovementQuality(id)) {
                record.U64("quality", id);
                record.U64("resolved", quality->resolved);
                record.U64("substituted", quality->substituted);
                record.U64("resets", quality->resets);
            }
        }
        digest_.Add(record);
    }

private:
    struct Driver final {
        std::uint64_t epoch{}, life{}, next{1};
        ActionId action{};
        std::set<ActionId> reported;
    };
    PvpMatch match_;
    Digest& digest_;
    WorldSnapshot snapshot_;
    std::map<PlayerId, Driver> drivers_;
};

// Movement scenarios -----------------------------------------------------------

using Pattern = std::function<Intent(unsigned tick)>;

void RunMovement(std::string_view arenaName, const Pattern& pattern, unsigned ticks, Digest& digest) {
    Duel duel(NamedArena(arenaName), digest, {1});
    for (unsigned t = 0; t < ticks; ++t) {
        duel.Move(1, pattern(t));
        duel.Tick();
    }
}

Intent Axial(unsigned t, unsigned scale) {
    const unsigned leg = (t / (45 * scale)) % 6;
    switch (leg) {
    case 0: return {1, 0};
    case 1: return {-1, 0};
    case 2: return {0, 1};
    case 3: return {0, -1};
    default: return {};
    }
}

// Combat scenarios ---------------------------------------------------------------

// Player 1 faces the target along +z (yaw 0) in the corridor and pillar arenas.
void RunDuelToKill(std::string_view arenaName, bool aimed, unsigned scale, Digest& digest) {
    Duel duel(NamedArena(arenaName), digest, {1, 2});
    const unsigned ticks = (60 + 4 * 15 + 200 + 120) * scale;
    for (unsigned t = 0; t < ticks; ++t) {
        duel.Move(1, {});
        duel.Move(2, {});
        if (t >= 60 && (t - 60) % 15 == 0 && t < 60 + 6 * 15) {
            if (aimed) {
                const auto [yaw, pitch] = duel.Aim(1, 2);
                duel.Act(1, ActionKind::Shot, yaw, pitch);
            } else duel.Act(1, ActionKind::Shot);
        }
        duel.Tick();
    }
}

void RunMagazine(unsigned scale, Digest& digest) {
    Duel duel(NamedArena("corridor"), digest, {1, 2});
    // Player 2 steps aside so every shot misses into the far wall.
    for (unsigned t = 0; t < 30; ++t) { duel.Move(2, {0, 1}); duel.Move(1, {}); duel.Tick(); }
    const unsigned ticks = 400 * scale;
    for (unsigned t = 0; t < ticks; ++t) {
        duel.Move(1, {});
        duel.Move(2, {});
        const unsigned phase = t % 400;
        if (phase < 160 && phase % 4 == 0) duel.Act(1, ActionKind::Shot);          // cooldown rejections, then empty
        if (phase == 170) duel.Act(1, ActionKind::Reload);
        if (phase == 175) duel.Act(1, ActionKind::Shot);                         // reloading
        if (phase == 180) duel.Act(1, ActionKind::Reload);                       // already reloading
        if (phase == 300) duel.Act(1, ActionKind::Reload);                       // magazine full
        duel.Tick();
    }
}

void RunCrossLife(unsigned scale, Digest& digest) {
    Duel duel(NamedArena("corridor"), digest, {1, 2});
    for (unsigned round = 0; round < scale; ++round) {
        for (unsigned t = 0; t < 400; ++t) {
            duel.Move(1, {});
            duel.Move(2, {});
            if (t >= 20 && t < 80 && (t - 20) % 12 == 0) duel.Act(1, ActionKind::Shot);   // kill player 2
            if (t == 120 || t == 130) duel.Act(1, ActionKind::Shot);                      // dead target
            if (t == 140) duel.Act(2, ActionKind::Shot);                                  // dead shooter
            if (t == 300) duel.Act(2, ActionKind::Shot, 0, 0, std::uint64_t{1});          // stale life after respawn
            duel.Tick();
        }
    }
}

void RunGraze(unsigned scale, Digest& digest) {
    Duel duel(NamedArena("corridor"), digest, {1, 2});
    const unsigned ticks = 300 * scale;
    for (unsigned t = 0; t < ticks; ++t) {
        duel.Move(1, {});
        duel.Move(2, {});
        if (t % 12 == 0) {
            // Sweep the aim across the target capsule's silhouette edge.
            const float offset = (static_cast<int>((t / 12) % 21) - 10) * 0.0035F;
            duel.Act(1, ActionKind::Shot, offset, 0.02F);
        }
        if (t % 120 == 119) duel.Act(1, ActionKind::Reload);
        duel.Tick();
    }
}

void RunRejoin(unsigned scale, Digest& digest) {
    Duel duel(NamedArena("corridor"), digest, {1, 2});
    for (unsigned round = 0; round < scale; ++round) {
        for (unsigned t = 0; t < 120; ++t) {
            duel.Move(1, {1, 0});
            duel.Move(2, {});
            if (t == 30) duel.Act(1, ActionKind::Shot);
            if (t == 60) duel.Rejoin(2);
            duel.Tick();
        }
    }
}

// Prediction over a deterministic virtual wire (millisecond event clock).
struct LinkOptions final { int rtt{}; bool impaired{}; bool stall{}; int fps{60}; };

void RunLink(LinkOptions options, unsigned scale, Digest& digest) {
    const auto arena = NamedArena("contract");
    PvpMatch match(arena);
    std::string error;
    Require(match.Join(1, error), error);
    LocalPlayerPrediction client(arena);
    client.Reconcile(match.Snapshot().players.front(), 0);
    struct InputPacket { double due; PlayerInput input; };
    struct SnapshotPacket { double due; WorldSnapshot snapshot; };
    std::vector<InputPacket> inputs;
    std::vector<SnapshotPacket> snapshots;
    std::uint32_t random = 0x130421;
    const auto next = [&] { random = random * 1664525U + 1013904223U; return random; };
    const auto delay = [&] { return std::max(0, options.rtt / 2 + (options.impaired ? static_cast<int>(next() % 21) - 10 : 0)); };
    unsigned inputNumber{}, snapshotNumber{};
    double nextFrame{}, nextSend{}, nextTick = 1000.0 / 60.0, previousFrame{};
    PlayerInput published;
    const int duration = 6000 * static_cast<int>(scale);
    for (int millisecond = 0; millisecond <= duration; ++millisecond) {
        const double now = millisecond;
        const int local = millisecond % 6000;
        const bool stalled = options.stall && local >= 2700 && local < 2950;
        std::stable_sort(inputs.begin(), inputs.end(), [](const auto& a, const auto& b) { return a.due < b.due; });
        while (!inputs.empty() && inputs.front().due <= now) {
            static_cast<void>(match.SubmitInput(inputs.front().input));
            inputs.erase(inputs.begin());
        }
        if (!stalled) {
            std::stable_sort(snapshots.begin(), snapshots.end(), [](const auto& a, const auto& b) { return a.due < b.due; });
            while (!snapshots.empty() && snapshots.front().due <= now) {
                const auto snapshot = std::move(snapshots.front().snapshot);
                snapshots.erase(snapshots.begin());
                client.Reconcile(snapshot.players.front(), snapshot.tick);
            }
        }
        if (now + 0.000001 >= nextFrame) {
            nextFrame += 1000.0 / options.fps;
            if (!stalled) {
                const float forward = (local >= 300 && local < 2300) || (local >= 3300 && local < 4000) ? 1.0F : 0.0F;
                const bool jump = local == 3500;
                if (client.Advance((now - previousFrame) / 1000.0, forward, 0, 0, 0, jump)) published = client.PendingInput();
                previousFrame = now;
                const auto& o = client.Observation();
                Record record("prediction");
                record.F64("ms", now);
                record.Vec("predictedPosition", o.predictedPosition);
                record.Vec("renderPosition", o.renderPosition);
                record.Vec("correctionOffset", o.correctionOffset);
                record.U64("lastResolvedCommand", o.lastResolvedCommand);
                record.U64("latestCommand", o.latestCommand);
                record.U64("authorityTick", o.authorityTick);
                record.U64("pendingCommands", o.pendingCommands);
                record.Bool("active", o.active);
                record.Bool("frozen", o.frozen);
                record.U64("movementEpoch", o.movementEpoch);
                record.U64("serverPendingCommands", o.serverPendingCommands);
                record.U64("previousCommand", o.previousCommand);
                record.U64("currentCommand", o.currentCommand);
                record.F32("interpolationAlpha", o.interpolationAlpha);
                record.F32("verticalVelocity", o.verticalVelocity);
                record.Bool("grounded", o.grounded);
                record.U64("lifeGeneration", o.lifeGeneration);
                record.U64("lifeState", static_cast<std::uint64_t>(o.lifeState));
                record.U64("phaseTracking", static_cast<std::uint64_t>(o.phaseTracking));
                record.U64("phaseCorrections", o.phaseCorrections);
                record.U64("phaseLateCorrections", o.phaseLateCorrections);
                digest.Add(record);
            }
        }
        if (now + 0.000001 >= nextSend) {
            nextSend += 1000.0 / InputSendRate;
            if (!published.commands.empty()) {
                ++inputNumber;
                const bool drop = options.impaired && (inputNumber == 1 || next() % 20 == 0);
                if (!drop) {
                    inputs.push_back({now + delay(), published});
                    if (options.impaired && inputNumber % 7 == 0) inputs.push_back({now + delay() + 45, published});
                }
            }
        }
        if (now + 0.000001 >= nextTick) {
            nextTick += 1000.0 / 60.0;
            match.Tick({match.TickCount() + 1, TickSeconds});
            Record record("match");
            const auto snapshot = match.Snapshot();
            EncodeSnapshot(record, snapshot);
            digest.Add(record);
            ++snapshotNumber;
            if (!options.impaired || next() % 20 != 0) {
                snapshots.push_back({now + delay(), snapshot});
                if (options.impaired && snapshotNumber % 11 == 0) snapshots.push_back({now + delay() + 12, snapshot});
            }
        }
    }
}

// Host scenarios over a fake clock ---------------------------------------------------

void RunHost(bool poorConnection, unsigned scale, Digest& digest) {
    auto clock = std::chrono::steady_clock::time_point{} + std::chrono::hours(1);
    MatchRuntimeHost host(NamedArena("corridor"), [&clock] { return clock; });
    Require(host.QueueJoin(1, 1) && host.QueueJoin(2, 2), "queue join");
    std::map<PlayerId, std::uint64_t> next{{1, 1}, {2, 1}}, epochs{{1, 1}, {2, 1}};
    std::map<PlayerId, ActionId> actions;
    std::optional<WorldSnapshot> latest;
    const unsigned ticks = (poorConnection ? 2400 : 900) * scale;
    for (unsigned t = 0; t < ticks; ++t) {
        for (const PlayerId id : {PlayerId{1}, PlayerId{2}}) {
            const PlayerState* state = nullptr;
            if (latest) for (const auto& p : latest->players) if (p.playerId == id) state = &p;
            if (!state || state->lifeState == LifeState::Dead) continue;
            if (state->movementEpoch != epochs[id]) { epochs[id] = state->movementEpoch; next[id] = state->lastResolvedCommand + 1; }
            // A poor connection reports snapshots it applied a quarter second ago.
            const auto observed = poorConnection && id == 2 && latest->tick > 15 ? latest->tick - 15 : latest->tick;
            const float forward = (t / 60) % 2 ? 1.0F : -1.0F;
            static_cast<void>(host.SubmitInput({id, {{next[id]++, forward, 0, 0, 0, t % 97 == 0}},
                state->movementEpoch, state->lifeGeneration, observed}));
            if (id == 1 && t % 25 == 0) {
                const auto kind = t % 300 == 275 ? ActionKind::Reload : ActionKind::Shot;
                static_cast<void>(host.SubmitActionBatch({id, {{++actions[id], latest->tick, 0, 0, kind,
                    state->lifeGeneration}}}, 0));
            }
        }
        clock += std::chrono::nanoseconds(1'000'000'000 / AuthorityTickRate);
        static_cast<void>(host.Advance(TickSeconds));
        if (auto snapshot = host.TakeSnapshot()) latest = std::move(snapshot);
        Record record("host");
        if (latest) EncodeSnapshot(record, *latest);
        for (const auto& control : host.TakeControlResults()) {
            record.U64("controlRequest", control.requestId);
            record.U64("controlPlayer", control.playerId);
            record.U64("controlKind", static_cast<std::uint64_t>(control.kind));
            record.Bool("controlAccepted", control.accepted);
            record.Text("controlError", control.error);
        }
        for (const auto& eviction : host.TakeEvictions()) {
            record.U64("evicted", eviction.playerId);
            record.U64("evictionReason", static_cast<std::uint64_t>(eviction.reason));
            record.U64("referenceAgeMillis", eviction.referenceAgeMillis);
            record.U64("substitutedPermille", eviction.substitutedPermille);
            record.U64("movementResets", eviction.movementResets);
        }
        for (const PlayerId id : {PlayerId{1}, PlayerId{2}}) {
            if (const auto results = host.GetActionResults(id)) {
                ActionId through = results->retiredThrough;
                for (const auto& decision : results->decisions) {
                    EncodeDecision(record, id, decision);
                    through = std::max(through, decision.actionId);
                }
                if (through > results->retiredThrough) static_cast<void>(host.QueueActionAcknowledgement(id, through));
            }
        }
        digest.Add(record);
    }
}

// Scenario table -------------------------------------------------------------------

struct Scenario final {
    std::string name;
    bool golden{};
    std::function<void(unsigned scale, Digest&)> run;
};

std::vector<Scenario> Scenarios() {
    std::vector<Scenario> list;
    for (const char* arena : {"contract", "pillar", "product", "corner"}) {
        const std::string a = arena;
        list.push_back({"move-axial/" + a, true, [a](unsigned s, Digest& d) {
            RunMovement(a, [s](unsigned t) { return Axial(t, s); }, 300 * s, d); }});
        list.push_back({"move-diagonal/" + a, true, [a](unsigned s, Digest& d) {
            RunMovement(a, [](unsigned t) { return t < 150 ? Intent{1, 1} : t < 300 ? Intent{-1, -1} : Intent{}; }, 330 * s, d); }});
        list.push_back({"move-jump/" + a, true, [a](unsigned s, Digest& d) {
            RunMovement(a, [](unsigned t) { return Intent{t >= 60 && t < 120 ? 1.0F : 0.0F, 0, 0, 0, t % 60 == 5}; }, 240 * s, d); }});
    }
    for (const char* arena : {"contract", "corner"}) {
        const std::string a = arena;
        list.push_back({"move-wall-slide/" + a, true, [a](unsigned s, Digest& d) {
            RunMovement(a, [](unsigned t) { return t < 200 ? Intent{1, 0.5F} : Intent{0.5F, -1}; }, 400 * s, d); }});
    }
    for (const char* arena : {"contract", "product"}) {
        const std::string a = arena;
        list.push_back({"move-turning/" + a, false, [a](unsigned s, Digest& d) {
            RunMovement(a, [](unsigned t) {
                return Intent{1, (t / 40) % 2 ? 0.5F : 0.0F, static_cast<float>(t) * 0.05F,
                              static_cast<float>((static_cast<int>(t % 80) - 40)) * 0.01F};
            }, 400 * s, d); }});
    }
    list.push_back({"move-held-gaps/contract", true, [](unsigned s, Digest& d) {
        RunMovement("contract", [](unsigned t) { Intent i{1, 0}; i.send = !(t >= 40 && t < 55) && !(t >= 100 && t < 140); return i; },
            240 * s, d); }});
    list.push_back({"move-backlog/contract", true, [](unsigned s, Digest& d) {
        Duel duel(NamedArena("contract"), d, {1});
        for (unsigned t = 0; t < 240 * s; ++t) {
            if (t < 60 && t % 2 == 0) duel.Flood(1, 6); else duel.Move(1, {1, 0});
            duel.Tick();
        }
    }});
    list.push_back({"combat-kill-respawn/corridor", true, [](unsigned s, Digest& d) { RunDuelToKill("corridor", false, s, d); }});
    list.push_back({"combat-kill-respawn-aimed/product", false, [](unsigned s, Digest& d) { RunDuelToKill("product", true, s, d); }});
    list.push_back({"combat-kill-respawn-aimed/contract", false, [](unsigned s, Digest& d) { RunDuelToKill("contract", true, s, d); }});
    list.push_back({"combat-occluded/pillar", true, [](unsigned s, Digest& d) { RunDuelToKill("pillar", false, s, d); }});
    list.push_back({"combat-magazine/corridor", true, [](unsigned s, Digest& d) { RunMagazine(s, d); }});
    list.push_back({"combat-cross-life/corridor", true, [](unsigned s, Digest& d) { RunCrossLife(s, d); }});
    list.push_back({"combat-graze/corridor", false, [](unsigned s, Digest& d) { RunGraze(s, d); }});
    list.push_back({"combat-rejoin/corridor", true, [](unsigned s, Digest& d) { RunRejoin(s, d); }});
    for (const auto& [name, options] : std::vector<std::pair<std::string, LinkOptions>>{
             {"rtt0", {0}}, {"rtt40", {40}}, {"rtt100", {100}}, {"impaired-rtt40", {40, true}},
             {"stall-rtt40", {40, false, true}}, {"fps30-rtt40", {40, false, false, 30}}, {"fps144-rtt40", {40, false, false, 144}}})
        list.push_back({"prediction-" + name + "/contract", true, [options](unsigned s, Digest& d) { RunLink(options, s, d); }});
    list.push_back({"host-play/corridor", true, [](unsigned s, Digest& d) { RunHost(false, s, d); }});
    list.push_back({"host-poor-connection/corridor", true, [](unsigned s, Digest& d) { RunHost(true, s, d); }});
    return list;
}

void Usage() {
    std::cout << "gyo_object_fps_pvp_authority_digest\n"
                 "  --list                         scenario names and golden flags\n"
                 "  --fields                       digest field list and version\n"
                 "  --run --out FILE [--scale N] [--ticks] [--only NAME]\n"
                 "  --dump NAME --out FILE [--scale N]   field-level records of one scenario\n"
                 "  --check-golden FILE            the golden subset must match FILE\n";
}

std::string JsonString(std::string_view text) {
    std::string out = "\"";
    for (const char c : text) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n') { out += "\\n"; continue; }
        out += c;
    }
    return out + '"';
}
} // namespace

int main(int argc, char** argv) {
    try {
        std::vector<std::string_view> args(argv + 1, argv + argc);
        const auto option = [&](std::string_view name) -> std::optional<std::string_view> {
            for (std::size_t i = 0; i + 1 < args.size(); ++i) if (args[i] == name) return args[i + 1];
            return std::nullopt;
        };
        const auto flag = [&](std::string_view name) { return std::find(args.begin(), args.end(), name) != args.end(); };
        const unsigned scale = option("--scale") ? static_cast<unsigned>(std::stoul(std::string(*option("--scale")))) : 1;
        Require(scale >= 1 && scale <= 10, "--scale must be 1..10");
        const auto scenarios = Scenarios();
        if (flag("--list")) {
            for (const auto& s : scenarios) std::cout << s.name << (s.golden ? " golden" : "") << '\n';
            return 0;
        }
        if (flag("--fields")) {
            std::cout << "digest_version=" << DigestVersion << '\n' << DigestFields;
            return 0;
        }
        if (const auto name = option("--dump")) {
            const auto out = option("--out");
            Require(out.has_value(), "--dump needs --out");
            std::ofstream file{std::string(*out)};
            for (const auto& s : scenarios) {
                if (s.name != *name) continue;
                Digest digest(s.golden, false, &file);
                s.run(scale, digest);
                return file ? 0 : 1;
            }
            throw std::runtime_error("Unknown scenario " + std::string(*name));
        }
        if (const auto golden = option("--check-golden")) {
            std::ifstream file{std::string(*golden)};
            Require(bool(file), "Cannot read golden file");
            std::map<std::string, std::string> expected;
            for (std::string line; std::getline(file, line);) {
                if (line.empty() || line.front() == '#') continue;
                std::istringstream fields(line);
                std::string name, value;
                Require(bool(fields >> name >> value), "Malformed golden line: " + line);
                expected[name] = value;
            }
            unsigned checked{}, mismatched{};
            for (const auto& s : scenarios) {
                if (!s.golden) continue;
                Digest digest(true, false, nullptr);
                s.run(1, digest);
                const auto found = expected.find(s.name);
                ++checked;
                if (found == expected.end() || found->second != digest.Value()) {
                    ++mismatched;
                    std::cerr << "golden mismatch: " << s.name << " expected "
                              << (found == expected.end() ? "<absent>" : found->second) << " got " << digest.Value() << '\n';
                }
            }
            Require(checked == expected.size(), "golden file names scenarios outside the golden subset");
            std::cout << checked << " golden scenarios checked, " << mismatched << " mismatched\n";
            return mismatched ? 1 : 0;
        }
        if (flag("--run")) {
            const auto out = option("--out");
            Require(out.has_value(), "--run needs --out");
            const bool ticks = flag("--ticks");
            const auto only = option("--only");
            std::ofstream file{std::string(*out)};
            file << "{\"digest_version\": " << DigestVersion << ", \"scale\": " << scale
                 << ", \"fields\": " << JsonString(DigestFields) << ", \"scenarios\": [\n";
            bool first = true;
            for (const auto& s : scenarios) {
                if (only && s.name != *only) continue;
                Digest digest(s.golden, ticks, nullptr);
                s.run(scale, digest);
                file << (first ? "" : ",\n") << "{\"name\": " << JsonString(s.name) << ", \"golden\": "
                     << (s.golden ? "true" : "false") << ", \"records\": " << digest.Count()
                     << ", \"digest\": " << JsonString(digest.Value());
                if (ticks) {
                    file << ", \"record_prefixes\": [";
                    for (std::size_t i = 0; i < digest.Prefixes().size(); ++i)
                        file << (i ? "," : "") << JsonString(digest.Prefixes()[i]);
                    file << ']';
                }
                file << '}';
                first = false;
            }
            file << "\n]}\n";
            return file ? 0 : 1;
        }
        Usage();
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "authority digest: " << error.what() << '\n';
        return 1;
    }
}
