// The core PvP script: a fixed sequence of inputs played by the game's three ends, printing the authority digest of
// every World at every frame (and, with --report, what the two ends compare in the handshake: the net registrations
// and the decision dataset). Two builds of the game print the same lines when they play the core PvP the same way:
// the A/B of the mode configuration (mode-config declaration, section 5) compares the output of the build before a
// change with the one after it, on one machine (the digest holds floats: it is not compared across machines).
//
//   gyo_object_fps_pvp_core_script [--report] [--check]
//
// --check: exits 1 unless the script went through every path it is there for (a head, a body and a leg hit, a kill,
// a respawn, a reload, a fall below the kill height, a landing that hurts, the Clients' walk and hit).
//
// Three parts, each a fresh Product on the shipped content:
//   lone   a Match without connections, its players put in and steered directly (walk, jump, crouch, the three parts,
//          a kill and the respawn, an empty magazine and the reload);
//   fall   the same on open ground without a floor plane: a fall below the kill height, a landing from ten metres;
//   net    a Match and two Clients over memory links: ready, walk, jump, crouch, the shots of one at the other until it
//          dies, its respawn;
//   local  a Local World: its player walks, jumps, fires and reloads.

#include "object_fps_pvp/world/PvpWorld.hpp"

#include "engine/authority/Consistency.hpp"
#include "engine/net/Link.hpp"
#include "engine/net/MemoryLink.hpp"
#include "engine/net/NetModule.hpp"
#include "engine/net/Replication.hpp"
#include "engine/physics_3d/Collider.hpp"
#include "engine/physics_3d/Shapes.hpp"
#include "engine/world_asset/WorldAssetModule.hpp"
#include "engine/world_time/Sampling.hpp"
#include "engine/world_time/TimeTypes.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <numbers>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace N = GYO::Engine::Net;
namespace A = GYO::Engine::Authority;
namespace P = GYO::Engine::Physics3d;
namespace WT = GYO::Engine::WorldTime;
namespace Math = GYO::Engine::Math;
namespace PW = object_fps_pvp::world;
namespace sim = object_fps_pvp::sim;

constexpr std::int64_t kStep = 15'625'000;
constexpr float kPi = std::numbers::pi_v<float>;

[[noreturn]] void Fail(const std::string& why) {
    std::cerr << "core script: " << why << "\n";
    std::exit(2);
}

void Must(const bool ok, const std::string& why) {
    if (!ok) Fail(why);
}

// The asset store's jobs at once, on the submitting thread: a World generates in the same frames every run.
class InlineJobs final : public GYO::Engine::Asset::AssetExecutor {
public:
    void Submit(Job job) override {
        queued_.push_back(std::move(job));
        if (running_) return;
        running_ = true;
        while (!queued_.empty()) {
            Job next = std::move(queued_.front());
            queued_.pop_front();
            next();
        }
        running_ = false;
    }

private:
    std::deque<Job> queued_;
    bool running_ = false;
};

InlineJobs& Jobs() {
    static InlineJobs jobs;
    return jobs;
}

void ReadShippedMaps(O::ProductDescription& description) {
    GYO::Engine::WorldAsset::AssetOptions assets;
    assets.contentRoot = PVP_CONTENT_ROOT;
    assets.executor = &Jobs();
    description.options.Set(assets);
    description.configure = [](O::ProductSetup& setup) { PW::ConfigureGame(setup); };
}

std::shared_ptr<const PW::GameContent> Shipped() {
    static const std::shared_ptr<const PW::GameContent> content = [] {
        std::string error;
        auto loaded = PW::LoadGameContent(PVP_CONTENT_ROOT, error);
        Must(loaded.has_value(), "the shipped content: " + error);
        return std::make_shared<const PW::GameContent>(std::move(*loaded));
    }();
    return content;
}

O::WorldDescription Stepped(O::WorldDescription world) {
    world.build = [inner = std::move(world.build)](O::WorldSetup& setup) {
        inner(setup);
        setup.Prepare([](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
            w.SetResource(WT::TimeSampling{kStep, WT::SamplingMode::ManualDelta});
            return {};
        });
    };
    return world;
}

std::unique_ptr<O::Product> Build(O::ProductDescription description) {
    auto built = O::Product::Build(std::move(description), O::MakeRegisteredUnits());
    if (!built) {
        std::string why = built.error().message + " " + built.error().detail;
        for (const O::UnitReportedError& error : built.error().unitErrors) why += "; " + error.code + " " + error.message;
        Fail("a Product does not build: " + why);
    }
    auto product = std::move(built).value();
    for (int frame = 0; frame < 64 && product->Generating("Game"); ++frame) Must(static_cast<bool>(product->RunFrame()), "a frame failed");
    Must(!product->Generating("Game"), "a World did not generate");
    return product;
}

K::World& GameWorld(O::Product& product) { return product.World("Game").GetWorld(); }
std::uint64_t TickOf(K::World& world) { return world.Resource<WT::Tick>()->value; }

std::string Hex(const std::uint64_t value) {
    char text[17];
    std::snprintf(text, sizeof text, "%016llx", static_cast<unsigned long long>(value));
    return text;
}

std::string DigestOf(K::World& world) {
    const auto digest = A::Digest(world);
    if (!digest) return "error:" + digest.error().message;
    return Hex(*digest);
}

PW::PlayerInput Input(const float forward, const float right, const float yaw, const float pitch = 0.0F) {
    PW::PlayerInput input;
    input.present = 1;
    input.ready = 1;
    input.forward = forward;
    input.right = right;
    input.yaw = yaw;
    input.pitch = pitch;
    return input;
}

PW::PlayerInput Pressed(PW::PlayerInput input, const bool jump, const bool fire, const bool reload, const bool crouch = false) {
    input.jump = jump ? 1 : 0;
    input.fire = fire ? 1 : 0;
    input.reload = reload ? 1 : 0;
    input.crouch = crouch ? 1 : 0;
    return input;
}

// What the script went through (--check).
struct Coverage {
    std::map<std::string, int> seen;
    void Note(const std::string& what) { ++seen[what]; }
    void Shot(const PW::ShotRecord& shot) {
        if (shot.kind != sim::ShotHitKind::Player) {
            Note("miss");
            return;
        }
        switch (shot.part) {
        case sim::HitPart::Head: Note("head"); break;
        case sim::HitPart::Body: Note("body"); break;
        case sim::HitPart::Leg: Note("leg"); break;
        default: Note("part?"); break;
        }
    }
};

// ---- lone: a Match the script steers directly

struct Lone {
    std::pair<N::MemoryLink, N::MemoryLink> links = N::MemoryLink::Pair({});
    std::shared_ptr<N::NetTime> clock = std::make_shared<N::NetTime>(0);
    std::unique_ptr<O::Product> product;
    K::EntityId next = 5'000'001;
    std::string label;

    Lone(std::string name, std::shared_ptr<const PW::GameContent> content) : label(std::move(name)) {
        O::ProductDescription description;
        description.name = "pvp-match";
        N::NetProductOptions net;
        net.role = N::NetRole::Server;
        net.link = std::make_shared<N::MemoryLink>(links.first);
        net.clock = [c = clock] { return *c; };
        net.seed = 7;
        auto seats = std::make_shared<PW::Seats>();
        net.admission = PW::SeatAdmission(seats);
        description.options.Set(net);
        ReadShippedMaps(description);
        description.worlds.push_back(Stepped(PW::MatchWorld({.content = std::move(content), .arena = 0, .seats = seats})));
        product = Build(std::move(description));
        Run(1);
    }
    K::World& World() { return GameWorld(*product); }
    K::EntityId Add(const Math::Vec3 feet, const float yaw) {
        const K::EntityId id = next++;
        PW::PlayerBody body;
        body.feet = feet;
        body.yaw = yaw;
        const sim::CombatRules rules;
        PW::PlayerCombat combat;
        combat.hp = rules.maximumHp;
        combat.ammo = rules.magazineCapacity;
        World().Insert(id, body);
        World().Insert(id, combat);
        World().Insert(id, PW::ShotRecord{});
        World().Insert(id, PW::HitRecord{});
        World().Insert(id, PW::Owner{id});
        World().Insert(id, PW::PlayerInput{});
        World().Insert(id, PW::PlayerMotion{});
        return id;
    }
    void Steer(const K::EntityId id, const PW::PlayerInput& input) { *World().GetMut<PW::PlayerInput>(id) = input; }
    void Run(const int frames) {
        for (int i = 0; i < frames; ++i) {
            Must(static_cast<bool>(product->RunFrame()), label + ": a frame failed");
            *clock += std::chrono::milliseconds(16);
            std::cout << label << " t=" << TickOf(World()) << " d=" << DigestOf(World()) << "\n";
        }
    }
    const PW::PlayerCombat& Combat(const K::EntityId id) { return *World().Get<PW::PlayerCombat>(id); }
    const PW::PlayerBody& Body(const K::EntityId id) { return *World().Get<PW::PlayerBody>(id); }
};

void PlayLone(Coverage& coverage) {
    Lone lone("lone", Shipped());
    // Walk to the +Z wall, a jump on the way, a crouched stretch.
    const K::EntityId walker = lone.Add({5, 0, 5}, 0.0F);
    lone.Steer(walker, Input(1, 0, 0));
    lone.Run(20);
    lone.Steer(walker, Pressed(Input(1, 0, 0), true, false, false));
    lone.Run(1);
    lone.Steer(walker, Input(1, 0, 0));
    lone.Run(40);
    lone.Steer(walker, Pressed(Input(1, 0.5F, 0.3F), false, false, false, true));
    lone.Run(30);
    lone.Steer(walker, Input(0, 0, 0.3F));
    lone.Run(10);
    lone.World().Despawn(walker);
    // B stands at (5, 0, 8) facing A at (5, 0, 3): a head, a body and a leg shot.
    const K::EntityId b = lone.Add({5, 0, 8}, kPi);
    lone.Run(4);
    const auto shootAt = [&](const float height, const float side) {
        const K::EntityId a = lone.Add({5, 0, 3}, 0.0F);
        const PW::PlayerInput aim = Input(0, 0, std::atan2(side, 5.0F), -std::atan2(height - 1.6F, 5.0F));
        lone.Steer(a, aim);
        lone.Run(2);
        lone.Steer(a, Pressed(aim, false, true, false));
        lone.Run(1);
        coverage.Shot(*lone.World().Get<PW::ShotRecord>(a));
        lone.World().Despawn(a);
    };
    shootAt(1.68F, 0.0F);
    shootAt(1.2F, 0.0F);
    shootAt(0.7F, 0.18F);
    // A keeps firing at B's chest: B dies, A empties the magazine and reloads, B respawns.
    const K::EntityId a = lone.Add({5, 0, 3}, 0.0F);
    const PW::PlayerInput chest = Input(0, 0, 0, -std::atan2(1.2F - 1.6F, 5.0F));
    bool died = false;
    std::uint32_t livesAtKill = 0;
    std::uint64_t respawn = 0;
    for (int round = 0; round < 40 && lone.Combat(a).ammo > 0; ++round) {
        lone.Steer(a, Pressed(chest, false, true, false));
        lone.Run(1);
        coverage.Shot(*lone.World().Get<PW::ShotRecord>(a));
        if (!died && lone.Combat(b).life == sim::LifeState::Dead) {
            died = true;
            livesAtKill = lone.Combat(b).lives;
            respawn = lone.Combat(b).respawnTick;
            coverage.Note("kill");
        }
        lone.Steer(a, chest);
        lone.Run(11);
    }
    if (lone.Combat(a).ammo == 0) coverage.Note("empty");
    lone.Steer(a, Pressed(chest, false, false, true));
    lone.Run(1);
    lone.Steer(a, chest);
    const std::uint64_t reloadEnd = lone.Combat(a).reloadEndTick;
    while (reloadEnd != 0 && TickOf(lone.World()) < reloadEnd + 2) lone.Run(1);
    if (reloadEnd != 0 && lone.Combat(a).ammo == sim::CombatRules{}.magazineCapacity) coverage.Note("reload");
    lone.Steer(a, Input(0, 0, 1.0F));
    while (died && TickOf(lone.World()) < respawn + 4) lone.Run(1);
    lone.Run(4);
    if (died && lone.Combat(b).lives > livesAtKill && lone.Combat(b).life == sim::LifeState::Alive) coverage.Note("respawn");
}

// ---- fall: open ground, no floor plane

void PlayFall(Coverage& coverage) {
    auto content = std::make_shared<PW::GameContent>(*Shipped());
    content->arenas.front().floor = std::nullopt;
    {
        Lone lone("fall", content);
        auto built = P::BoxSet::Build(std::vector<Math::Aabb>{{{100.0F, 0.0F, 100.0F}, {101.0F, 1.0F, 101.0F}}});
        Must(static_cast<bool>(built), "the open walls");
        lone.World().Insert(PW::kArenaEntity, P::Collider{P::Shape{std::move(*built)}});
        const K::EntityId faller = lone.Add({5, 0, 5}, 0.0F);
        lone.Steer(faller, Input(0, 0, 0));
        for (int step = 0; step < 64 * 5 && lone.Combat(faller).life == sim::LifeState::Alive; ++step) lone.Run(1);
        if (lone.Combat(faller).life == sim::LifeState::Dead) coverage.Note("fell");
        lone.Run(8);
    }
    {
        Lone lone("land", content);
        auto built = P::BoxSet::Build(std::vector<Math::Aabb>{{{100.0F, 0.0F, 100.0F}, {101.0F, 1.0F, 101.0F}},
                                                              {{0.0F, -1.0F, 0.0F}, {20.0F, 0.0F, 20.0F}}});
        Must(static_cast<bool>(built), "the ground");
        lone.World().Insert(PW::kArenaEntity, P::Collider{P::Shape{std::move(*built)}});
        const K::EntityId id = lone.Add({5, 10, 5}, 0.0F);
        lone.World().GetMut<PW::PlayerBody>(id)->grounded = 0;
        lone.Steer(id, Input(0, 0, 0));
        lone.Run(64 * 3);
        const PW::PlayerCombat& combat = lone.Combat(id);
        if (combat.life == sim::LifeState::Alive && combat.hp < sim::CombatRules{}.maximumHp) coverage.Note("landing");
    }
}

// ---- net: a Match and two Clients over memory links

class HubLink final : public N::Link {
public:
    explicit HubLink(std::vector<N::MemoryLink> ends) : ends_(std::move(ends)) {}
    N::NetResult<void> Send(const N::Endpoint to, const std::span<const std::byte> datagram) override {
        const std::size_t index = static_cast<std::size_t>(to.value - 100);
        if (to.value < 100 || index >= ends_.size()) {
            return GYO::Engine::Base::Err(N::NetFailure::Make(N::NetFailureCode::LinkFailed, "no such client"));
        }
        return ends_[index].Send(ends_[index].Remote(), datagram);
    }
    N::NetResult<std::optional<N::ReceivedDatagram>> Receive(const std::span<std::byte> buffer) override {
        for (std::size_t i = 0; i < ends_.size(); ++i) {
            auto received = ends_[i].Receive(buffer);
            if (!received) return received;
            if (received->has_value()) {
                return std::optional<N::ReceivedDatagram>{N::ReceivedDatagram{(*received)->size, N::Endpoint{100 + i}}};
            }
        }
        return std::optional<N::ReceivedDatagram>{};
    }

private:
    std::vector<N::MemoryLink> ends_;
};

struct Net {
    std::shared_ptr<N::NetTime> clock = std::make_shared<N::NetTime>(0);
    std::vector<std::pair<N::MemoryLink, N::MemoryLink>> links;
    std::unique_ptr<O::Product> match;
    std::vector<std::unique_ptr<O::Product>> clients;

    Net() {
        auto seats = std::make_shared<PW::Seats>(4);
        std::vector<N::MemoryLink> serverEnds;
        for (int i = 0; i < 2; ++i) {
            N::MemoryLinkSettings settings;
            settings.seed = static_cast<std::uint64_t>(60 + i);
            settings.aToB.fixedDelaySteps = 2;
            settings.bToA.fixedDelaySteps = 2;
            links.push_back(N::MemoryLink::Pair(settings));
            serverEnds.push_back(links.back().first);
        }
        auto reader = [c = clock] { return *c; };
        {
            O::ProductDescription description;
            description.name = "pvp-match";
            N::NetProductOptions net;
            net.role = N::NetRole::Server;
            net.link = std::make_shared<HubLink>(serverEnds);
            net.clock = reader;
            net.seed = 7;
            net.admission = PW::SeatAdmission(seats);
            description.options.Set(net);
            ReadShippedMaps(description);
            description.worlds.push_back(Stepped(PW::MatchWorld({.content = Shipped(), .arena = 0, .seats = seats, .expectUsers = {1, 2}})));
            match = Build(std::move(description));
        }
        for (int i = 0; i < 2; ++i) {
            const auto user = static_cast<std::uint64_t>(i + 1);
            O::ProductDescription description;
            description.name = "pvp-client";
            N::NetProductOptions net;
            net.role = N::NetRole::Client;
            net.link = std::make_shared<N::MemoryLink>(links[static_cast<std::size_t>(i)].second);
            net.clock = reader;
            description.options.Set(net);
            ReadShippedMaps(description);
            O::WorldDescription world = Stepped(PW::ClientWorld({.content = Shipped(), .arena = 0, .localUser = user}));
            world.prepare.push_back([target = N::NetConnectTarget{N::MemoryLink::EndpointA(), user, {}}](K::World& w)
                                        -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
                w.SetResource(target);
                return {};
            });
            description.worlds.push_back(std::move(world));
            clients.push_back(Build(std::move(description)));
        }
    }
    K::World& Match() { return GameWorld(*match); }
    K::World& Client(const std::size_t i) { return GameWorld(*clients[i]); }
    void Frame(const std::vector<std::optional<PW::PlayerInput>>& inputs) {
        for (std::size_t i = 0; i < clients.size(); ++i) {
            if (i < inputs.size() && inputs[i]) {
                if (auto* local = N::LocalInput<PW::PlayerInput>(Client(i))) {
                    const auto target = static_cast<std::uint32_t>(TickOf(Client(i)) + 1);
                    local->Set(target, *inputs[i], N::ViewTime{target, 0});
                }
            }
        }
        Must(static_cast<bool>(match->RunFrame()), "net: a Match frame failed");
        for (auto& client : clients) Must(static_cast<bool>(client->RunFrame()), "net: a Client frame failed");
        for (auto& link : links) link.first.Advance();
        *clock += std::chrono::milliseconds(16);
        std::cout << "net m t=" << TickOf(Match()) << " d=" << DigestOf(Match());
        for (std::size_t i = 0; i < clients.size(); ++i) std::cout << " c" << i + 1 << " t=" << TickOf(Client(i)) << " d=" << DigestOf(Client(i));
        std::cout << "\n";
    }
    void Frames(const int n, const std::vector<std::optional<PW::PlayerInput>>& inputs = {}) {
        for (int i = 0; i < n; ++i) Frame(inputs);
    }
};

K::EntityId PlayerOf(K::World& world, const std::uint64_t user) {
    for (const K::EntityId id : world.Entities()) {
        const PW::Owner* owner = world.Get<PW::Owner>(id);
        if (owner != nullptr && owner->userId == user) return id;
    }
    return 0;
}

void PlayNet(Coverage& coverage, const bool report) {
    Net net;
    if (report) {
        const auto print = [](const char* end, K::World& world) {
            const N::NetWorldLink* link = world.Resource<N::NetWorldLink>();
            Must(link != nullptr && link->registry != nullptr, "no net registrations");
            std::cout << "report " << end << " protocol " << Hex(link->registry->Hash()) << "\n";
            for (const auto& e : link->registry->components) std::cout << "report " << end << " c " << e->name << " v" << e->version << "\n";
            for (const auto& e : link->registry->resources) std::cout << "report " << end << " r " << e->name << " v" << e->version << "\n";
            for (const auto& e : link->registry->messages) std::cout << "report " << end << " m " << e->name << " v" << e->version << "\n";
            for (const auto& e : link->registry->inputs) std::cout << "report " << end << " i " << e->name << " v" << e->version << "\n";
            const auto dataset = A::DatasetReport(world);
            std::istringstream lines(dataset ? *dataset : "error: " + dataset.error().message);
            for (std::string line; std::getline(lines, line);) std::cout << "report " << end << " dataset " << line << "\n";
        };
        print("match", net.Match());
        print("client", net.Client(0));
    }
    // Waiting, then both ready.
    net.Frames(60, {Input(0, 0, 0), Input(0, 0, 0)});
    const K::EntityId one = PlayerOf(net.Match(), 1);
    const K::EntityId two = PlayerOf(net.Match(), 2);
    Must(one != 0 && two != 0, "net: the players are not seated");
    net.Frames(20, {Input(0, 0, 0), Input(0, 0, 0)});
    // One walks and jumps; the other crouches.
    const float yaw = net.Match().Get<PW::PlayerBody>(one)->yaw;
    const Math::Vec3 start = net.Match().Get<PW::PlayerBody>(one)->feet;
    net.Frames(20, {Input(1, 0, yaw), Pressed(Input(0, 0, 0), false, false, false, true)});
    net.Frame({Pressed(Input(1, 0, yaw), true, false, false), Pressed(Input(0, 0, 0), false, false, false, true)});
    net.Frames(30, {Input(1, 0, yaw), Input(0, 0, 0)});
    net.Frames(40, {Input(0, 0, yaw), Input(0, 0, 0)});
    if (Math::Length(net.Client(1).Get<PW::PlayerBody>(one)->feet - start) > 0.5F) coverage.Note("net-walk");
    // One shoots two's chest every cooldown until two dies, then waits for the respawn.
    bool died = false;
    for (int round = 0; round < 30 && !died; ++round) {
        const Math::Vec3 from = net.Match().Get<PW::PlayerBody>(one)->feet;
        const Math::Vec3 to = net.Match().Get<PW::PlayerBody>(two)->feet;
        const Math::Vec3 aimAt = to + Math::Vec3{0, 1.2F, 0} - (from + Math::Vec3{0, 1.6F, 0});
        const float aimYaw = std::atan2(aimAt.x, aimAt.z);
        const float aimPitch = -std::atan2(aimAt.y, std::sqrt(aimAt.x * aimAt.x + aimAt.z * aimAt.z));
        net.Frames(4, {Input(0, 0, aimYaw, aimPitch), Input(0, 0, 0)});
        net.Frame({Pressed(Input(0, 0, aimYaw, aimPitch), false, true, false), Input(0, 0, 0)});
        net.Frames(10, {Input(0, 0, aimYaw, aimPitch), Input(0, 0, 0)});
        const PW::HitRecord* seen = net.Client(1).Get<PW::HitRecord>(two);
        if (seen != nullptr && seen->hits > 0) coverage.Note("net-hit");
        died = net.Match().Get<PW::PlayerCombat>(two)->life == sim::LifeState::Dead;
    }
    if (died) coverage.Note("net-kill");
    net.Frames(64 * 3 + 40, {Input(0, 0, 0), Input(0, 0, 0)});
    if (died && net.Match().Get<PW::PlayerCombat>(two)->lives == 2) coverage.Note("net-respawn");
}

// ---- local: a Local World

void PlayLocal(Coverage& coverage, const bool report) {
    O::ProductDescription description;
    description.name = "pvp-solo";
    ReadShippedMaps(description);
    description.worlds.push_back(Stepped(PW::LocalWorld({.content = Shipped(), .arena = 0, .localUser = 9, .spawn = 1})));
    auto product = Build(std::move(description));
    K::World& world = GameWorld(*product);
    if (report) {
        const auto dataset = A::DatasetReport(world);
        std::istringstream lines(dataset ? *dataset : "error: " + dataset.error().message);
        for (std::string line; std::getline(lines, line);) std::cout << "report local dataset " << line << "\n";
    }
    const auto frame = [&] {
        Must(static_cast<bool>(product->RunFrame()), "local: a frame failed");
        std::cout << "local t=" << TickOf(world) << " d=" << DigestOf(world) << "\n";
    };
    frame();
    const K::EntityId player = PlayerOf(world, 9);
    Must(player != 0, "local: no player");
    const float yaw = world.Get<PW::PlayerBody>(player)->yaw;
    for (int i = 0; i < 160; ++i) {
        PW::PlayerInput input = Input(i < 80 ? 1.0F : 0.0F, i % 40 < 20 ? 0.0F : 0.5F, yaw + 0.01F * static_cast<float>(i));
        input = Pressed(input, i == 30, i % 12 == 0 && i < 120, i == 130, i >= 100 && i < 110);
        world.Insert(player, input);
        frame();
    }
    if (world.Get<PW::PlayerCombat>(player)->shots > 0) coverage.Note("local-shots");
}

} // namespace

int main(const int argc, char** argv) {
    bool report = false;
    bool check = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--report") {
            report = true;
        } else if (arg == "--check") {
            check = true;
        } else {
            std::cerr << "usage: gyo_object_fps_pvp_core_script [--report] [--check]\n";
            return 2;
        }
    }
    Coverage coverage;
    PlayLone(coverage);
    PlayFall(coverage);
    PlayNet(coverage, report);
    PlayLocal(coverage, report);
    std::cout << "coverage";
    for (const auto& [what, count] : coverage.seen) std::cout << " " << what << "=" << count;
    std::cout << "\n";
    if (!check) return 0;
    int missing = 0;
    for (const char* what : {"head", "body", "leg", "kill", "empty", "reload", "respawn", "fell", "landing", "net-walk", "net-hit", "net-kill",
                             "net-respawn", "local-shots"}) {
        if (!coverage.seen.contains(what)) {
            std::cerr << "core script: did not go through " << what << "\n";
            ++missing;
        }
    }
    return missing == 0 ? 0 : 1;
}
