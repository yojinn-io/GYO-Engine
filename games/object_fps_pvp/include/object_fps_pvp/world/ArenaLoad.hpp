#pragma once

// An arena's map, read by the World that plays it while the World generates (big-map declaration sections 15.2 point
// 2 and 15.6): the map's index is requested from the Product's asset store, which reads the regions the index names;
// once every region is there and the whole map is valid, what the World makes of it (the Match's walls, a Client's
// pictures) is made on a worker, and the World holds its generation until then. A World that fails to read its map
// stays generating and says why (MapLoad): what to do about it is the program's.

#include "object_fps_pvp/world/Components.hpp"

#include "engine/asset/AssetExecutor.hpp"
#include "engine/asset/Handle.hpp"
#include "engine/voxel_map/VoxelMapLoaders.hpp"
#include "engine/world_asset/AssetView.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace object_fps_pvp::world {

enum class MapStage : std::uint8_t {
    // The index and its regions are being read.
    Reading,
    // Every region is there and valid: what the World makes of the map is being made.
    Building,
    Done,
    Failed,
};

// How far a World has come with its map. Written by the World's map System; the program reads it (a loading screen,
// a Match that ends when its map does not read).
struct MapLoad final {
    // The regions read and the map's regions (0 until the index is read).
    std::uint32_t done = 0;
    std::uint32_t total = 0;
    MapStage stage = MapStage::Reading;
    // Failed: why (a region missing or not the index's, a spawn inside a wall).
    std::string failure;

    friend bool operator==(const MapLoad&, const MapLoad&) = default;
};
void HashValue(K::Hasher&, const MapLoad&);

// What a World's asset view has of a map now. complete: every region is there and the whole map is valid
// (CheckVoxelMap); index and regions (one per index entry, in index order) then point into the view, valid only while
// the System that read them runs. failure: why the map cannot be read (the asset store's failure, or a rule of the
// map).
struct MapReading final {
    std::uint32_t done = 0;
    std::uint32_t total = 0;
    bool complete = false;
    const GYO::Engine::VoxelMap::VoxelMapDocument* index = nullptr;
    std::vector<const GYO::Engine::VoxelMap::VoxelRegionDocument*> regions;
    std::string failure;
};
[[nodiscard]] MapReading ReadMap(const GYO::Engine::WorldAsset::AssetView& view,
                                 const GYO::Engine::Asset::Handle<GYO::Engine::VoxelMap::VoxelMapAsset>& map);

// A value made on an executor and taken by a System once it is there (Bevy's Task, polled each frame:
// bevy/examples/async_tasks/async_compute.rs:66, :128). Without an executor it is made at once, on the caller's thread.
// The job owns what it reads; a World gone before the job ends leaves only the job. Dropping the task cancels a job
// that has not started (Bevy's Task cancels on drop: bevy_tasks/src/task_pool.rs:558); one already running finishes
// and its value is dropped.
template <class T>
class WorkerTask final {
public:
    WorkerTask() = default;
    WorkerTask(const WorkerTask&) = delete;
    WorkerTask& operator=(const WorkerTask&) = delete;
    WorkerTask(WorkerTask&&) noexcept = default;
    WorkerTask& operator=(WorkerTask&& other) noexcept {
        if (this != &other) {
            Cancel();
            state_ = std::move(other.state_);
        }
        return *this;
    }
    ~WorkerTask() { Cancel(); }

    // make: T(), not throwing.
    template <class F>
    [[nodiscard]] static WorkerTask Start(GYO::Engine::Asset::AssetExecutor* executor, F make) {
        WorkerTask task;
        task.state_ = std::make_shared<State>();
        auto job = [state = task.state_, make = std::move(make)]() mutable {
            {
                const std::lock_guard lock(state->mutex);
                if (state->cancelled) return;
            }
            T value = make();
            const std::lock_guard lock(state->mutex);
            if (!state->cancelled) state->value.emplace(std::move(value));
        };
        if (executor != nullptr) {
            executor->Submit(std::move(job));
        } else {
            job();
        }
        return task;
    }

    // The value, once made: taken once (the task is then empty); nullopt before.
    [[nodiscard]] std::optional<T> Take() {
        if (state_ == nullptr) return std::nullopt;
        std::optional<T> value;
        {
            const std::lock_guard lock(state_->mutex);
            value.swap(state_->value);
        }
        if (value) state_.reset();
        return value;
    }

private:
    struct State {
        std::mutex mutex;
        bool cancelled = false;
        std::optional<T> value;
    };

    void Cancel() noexcept {
        if (state_ == nullptr) return;
        const std::lock_guard lock(state_->mutex);
        state_->cancelled = true;
        state_->value.reset();
    }

    std::shared_ptr<State> state_;
};

} // namespace object_fps_pvp::world
