#include "RetroFPS/Pvp/Arena.hpp"
#include "RetroFPS/Collision/CharacterCollision.hpp"
#include "RetroFPS/Pvp/Movement.hpp"
#include "RetroFPS/Pvp/NativeFiles.hpp"
#include "engine/math/geometry/Aabb.hpp"
#include "engine/base/Fnv1a.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/base/Error.hpp"
#include "engine/base/Assert.hpp"
#include "engine/os/Files.hpp"

#include <nlohmann/json.hpp>
#include <bit>
#include <cmath>
#include <span>
#include <stdexcept>
#include <string>

namespace fps::pvp {
namespace {
// A plain file name next to the file that names it (the arenas.json rule).
std::filesystem::path SiblingFile(const std::filesystem::path& owner, const std::string& file, const char* what) {
    if (file.empty() || file.find_first_of("/\\:") != std::string::npos || file == "." || file == "..")
        throw std::runtime_error(std::string(what) + " is a plain file name: " + file);
    auto name = Engine::Os::Files::PathFromUtf8(file);
    if (!name) throw std::runtime_error(std::string(what) + " " + file + ": " + Engine::Base::Describe(name.error()));
    return owner.parent_path() / *name;
}

Engine::Math::Vec3 ReadPosition(const nlohmann::json& value) {
    if (!value.is_array() || value.size() != 3)
        throw std::invalid_argument("Arena vectors require three numbers");
    return {value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>()};
}
}

namespace {
// Every Validate rule; on success, the wall grid built while checking the
// spawns, which Load keeps (one build per load).
std::shared_ptr<const Engine::Collision::StaticAabbGrid> CheckedWallGrid(const Arena& arena, std::string& error) {
    const auto& [version, id, width, depth, cellSize, movementSpeed, bodyHeight, radius, eyeHeight,
        walls, spawns, jumpHeight, gravity, wallGrid] = arena;
    (void)wallGrid;
    error.clear();
    const auto positive = [](float value) { return std::isfinite(value) && value > 0; };
    if (version != 1 || id.empty() || id.size() > 64 ||
        !positive(width) || !positive(depth) || !positive(cellSize) ||
        !positive(movementSpeed) || !positive(bodyHeight) || !positive(radius) ||
        !positive(eyeHeight) || !positive(jumpHeight) || !positive(gravity) ||
        !std::isfinite(2 * gravity * jumpHeight) ||
        !Engine::Collision::IsValid(Engine::Collision::VerticalCapsule{{}, bodyHeight, radius}) ||
        eyeHeight > bodyHeight ||
        spawns.size() < MinimumArenaSpawns || spawns.size() > MaximumArenaSpawns) {
        error = "Arena v1 requires an id, valid dimensions/movement settings and 2-64 spawns";
        return nullptr;
    }
    if (walls.size() > MaximumArenaColliders) {
        error = "Arena has " + std::to_string(walls.size()) + " colliders; at most " +
                std::to_string(MaximumArenaColliders);
        return nullptr;
    }
    for (const auto& wall : walls) {
        // Engine validity allows zero thickness; an empty wall is a content error.
        if (!Engine::Collision::IsValid(wall) ||
            wall.minimum.x >= wall.maximum.x || wall.minimum.y >= wall.maximum.y ||
            wall.minimum.z >= wall.maximum.z) {
            error = "Arena wall must be a finite non-empty AABB";
            return nullptr;
        }
    }
    // Content can exceed the grid's limits (registrations): an error, never
    // the constructor's assert. The spawn rules ask the grid Movement uses.
    auto built = Engine::Collision::StaticAabbGrid::Build(walls, cellSize);
    if (!built) {
        error = "Arena colliders do not fit the collision grid: " + Engine::Base::Describe(built.error());
        return nullptr;
    }
    auto grid = std::make_shared<const Engine::Collision::StaticAabbGrid>(std::move(*built));
    std::vector<Engine::Collision::VerticalCapsule> bodies;
    for (const auto& spawn : spawns) {
        const Engine::Collision::VerticalCapsule body{spawn.position, bodyHeight, radius};
        // On the floor, or on a wall top where Movement would ground it (one rule).
        if (!Engine::Math::IsFinite(body.feet) || spawn.position.y < 0 ||
            (spawn.position.y != 0 && !IsCharacterSupported(body, *grid)) || !std::isfinite(spawn.yaw) ||
            spawn.position.x < radius || spawn.position.x > width - radius ||
            spawn.position.z < radius || spawn.position.z > depth - radius ||
            !CanPlaceCharacterBody(body, *grid, bodies)) {
            error = "Arena spawn must stand on the floor or a wall top, inside the arena and free of walls/other spawns";
            return nullptr;
        }
        bodies.push_back(body);
    }
    return grid;
}
} // namespace

bool Arena::Validate(std::string& error) const { return CheckedWallGrid(*this, error) != nullptr; }

std::optional<Arena> Arena::Load(const std::filesystem::path& path, std::string& error,
                                 Engine::VoxelMesh::VoxelMeshGeometry* geometry) {
    error.clear();
    if (geometry) *geometry = {};
    try {
        NativeReadFailure failure;
        const auto bytes = ReadNativeFile(path, failure, 256 * 1024);
        if (!bytes && failure.tooLarge) throw std::runtime_error("Arena content exceeds 256 KiB");
        if (!bytes) throw std::runtime_error("Cannot open arena: " + Engine::Os::Files::PathForMessage(path));
        const auto json = nlohmann::json::parse(bytes->begin(), bytes->end());
        if (!json.at("version").is_number_integer()) throw std::runtime_error("Unsupported arena version");
        const auto schema = json.at("version").get<std::int64_t>();
        if (schema != 1 && schema != 2) throw std::runtime_error("Unsupported arena version");
        if (schema == 1 && !json.at("walls").is_array()) throw std::runtime_error("Arena walls must be an array");
        if (schema == 2 && json.contains("walls"))
            throw std::runtime_error("Arena schema 2 takes its walls from its geometry, not from walls");
        if (!json.at("spawns").is_array()) throw std::runtime_error("Arena spawns must be an array");
        Arena arena;
        Engine::VoxelMesh::VoxelMeshGeometry baked;
        arena.id = json.at("id").get<std::string>();
        arena.width = json.at("width").get<float>();
        arena.depth = json.at("depth").get<float>();
        arena.cellSize = json.value("cell_size", 1.0F);
        arena.movementSpeed = json.value("movement_speed", 3.0F);
        arena.bodyHeight = json.value("body_height", 1.8F);
        arena.radius = json.value("radius", 0.25F);
        arena.eyeHeight = json.value("eye_height", 1.6F);
        arena.jumpHeight = json.value("jump_height", 0.6F);
        arena.gravity = json.value("gravity", 18.0F);
        if (schema == 1) {
            for (const auto& wall : json.at("walls")) {
                const auto minimum = ReadPosition(wall.at("min"));
                const auto maximum = ReadPosition(wall.at("max"));
                arena.walls.push_back({minimum, maximum});
            }
        } else {
            // The baked map: Engine's gyo.voxel_mesh is its one validator.
            const auto& name = json.at("geometry");
            if (!name.is_string()) throw std::runtime_error("Arena geometry is a file name");
            const auto meshPath = SiblingFile(path, name.get<std::string>(), "Arena geometry");
            NativeReadFailure meshFailure;
            const auto meshBytes = ReadNativeFile(meshPath, meshFailure, Engine::VoxelMesh::MaxTextBytes);
            if (!meshBytes && meshFailure.tooLarge) throw std::runtime_error("Arena geometry exceeds 16 MiB");
            if (!meshBytes)
                throw std::runtime_error("Cannot open arena geometry: " + Engine::Os::Files::PathForMessage(meshPath));
            const auto mesh = Engine::VoxelMesh::ParseVoxelMesh(std::string_view(meshBytes->data(), meshBytes->size()));
            if (!mesh) throw std::runtime_error("Arena geometry: " + Engine::Base::Describe(mesh.error()));
            baked = Engine::VoxelMesh::ExpandVoxelMesh(*mesh);
            arena.walls = baked.colliders;
        }
        for (const auto& spawn : json.at("spawns"))
            arena.spawns.push_back({ReadPosition(spawn.at("position")), spawn.at("yaw").get<float>()});
        arena.wallGrid = CheckedWallGrid(arena, error);
        if (!arena.wallGrid) return std::nullopt;
        // Handed out only with an arena that loaded: a failure leaves it empty.
        if (geometry) *geometry = std::move(baked);
        return arena;
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

void Arena::BuildWallGrid() {
    wallGrid = std::make_shared<const Engine::Collision::StaticAabbGrid>(walls, cellSize);
}

const Engine::Collision::StaticAabbGrid& Arena::WallGrid() const {
    // An arena without walls needs no build (synthetic arenas in tests).
    static const Engine::Collision::StaticAabbGrid none({}, 1.0F);
    if (!wallGrid && walls.empty()) return none;
    GYO_ASSERT(wallGrid != nullptr && wallGrid->Size() == walls.size());
    return *wallGrid;
}

std::uint64_t ArenaContentDigest(const Arena& arena) {
    // Fourteen names: adding, removing or reordering a member of Arena breaks
    // this binding, so the canonical form changes with it (a wire change).
    // wallGrid is derived from walls and is not part of the content.
    const auto& [version, id, width, depth, cellSize, movementSpeed, bodyHeight, radius, eyeHeight,
        walls, spawns, jumpHeight, gravity, wallGrid] = arena;
    (void)wallGrid;
    std::string bytes;
    const auto u32 = [&](std::uint32_t value) {
        for (int shift = 24; shift >= 0; shift -= 8) bytes.push_back(static_cast<char>((value >> shift) & 0xffU));
    };
    const auto f32 = [&](float value) { u32(std::bit_cast<std::uint32_t>(value)); };
    const auto vec3 = [&](const Engine::Math::Vec3& value) { f32(value.x); f32(value.y); f32(value.z); };
    u32(version);
    u32(static_cast<std::uint32_t>(id.size()));
    bytes += id;
    for (const float value : {width, depth, cellSize, movementSpeed, bodyHeight, radius, eyeHeight}) f32(value);
    u32(static_cast<std::uint32_t>(walls.size()));
    for (const auto& wall : walls) { vec3(wall.minimum); vec3(wall.maximum); }
    u32(static_cast<std::uint32_t>(spawns.size()));
    for (const auto& spawn : spawns) { vec3(spawn.position); f32(spawn.yaw); }
    f32(jumpHeight);
    f32(gravity);
    return Engine::Base::Fnv1a64(bytes);
}

std::optional<std::vector<Arena>> LoadInstalledArenas(const std::filesystem::path& list, std::string& error,
                                                      std::vector<Engine::VoxelMesh::VoxelMeshGeometry>* geometries) {
    error.clear();
    if (geometries) geometries->clear();
    try {
        NativeReadFailure failure;
        const auto bytes = ReadNativeFile(list, failure);
        if (!bytes) throw std::runtime_error("Cannot open arena list: " + Engine::Os::Files::PathForMessage(list));
        const auto json = nlohmann::json::parse(bytes->begin(), bytes->end());
        if (!json.at("version").is_number_integer() || json.at("version").get<std::int64_t>() != 1)
            throw std::runtime_error("Unsupported arena list version");
        const auto& names = json.at("arenas");
        if (!names.is_array() || names.empty() || names.size() > 16)
            throw std::runtime_error("The arena list names 1-16 arenas");
        std::vector<Arena> arenas;
        for (const auto& name : names) {
            const auto file = name.get<std::string>();
            if (file.empty() || file.find_first_of("/\\:") != std::string::npos || file == "." || file == "..")
                throw std::runtime_error("Arena list entries are plain file names: " + file);
            // The name is JSON text, so UTF-8; never read in the ANSI code page.
            auto name8 = Engine::Os::Files::PathFromUtf8(file);
            if (!name8) throw std::runtime_error("Arena list entry " + file + ": " + Engine::Base::Describe(name8.error()));
            Engine::VoxelMesh::VoxelMeshGeometry geometry;
            auto arena = Arena::Load(list.parent_path() / *name8, error, geometries ? &geometry : nullptr);
            if (!arena) throw std::runtime_error(error);
            for (const auto& other : arenas)
                if (other.id == arena->id) throw std::runtime_error("Arena installed twice: " + arena->id);
            if (!arenas.empty() && arena->bodyHeight != arenas.front().bodyHeight)
                throw std::runtime_error("Installed arenas must share one body height: " + arena->id);
            arenas.push_back(std::move(*arena));
            if (geometries) geometries->push_back(std::move(geometry));
        }
        return arenas;
    } catch (const std::exception& exception) {
        error = "arena list " + Engine::Os::Files::PathForMessage(list) + ": " + exception.what();
        if (geometries) geometries->clear();
        return std::nullopt;
    }
}

bool ArenaHostsRoom(const Arena& arena, std::string& error) {
    error.clear();
    if (arena.spawns.size() < MaxPlayers) {
        error = "Arena " + arena.id + " has " + std::to_string(arena.spawns.size()) +
            " spawns; a room of " + std::to_string(MaxPlayers) + " players needs at least that many";
        return false;
    }
    return true;
}

} // namespace fps::pvp
