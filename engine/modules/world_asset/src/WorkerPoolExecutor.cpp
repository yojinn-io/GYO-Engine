#include "engine/world_asset/WorkerPoolExecutor.hpp"

#include <utility>

namespace GYO::Engine::WorldAsset {

WorkerPoolExecutor::WorkerPoolExecutor(Threads::WorkerPool& pool) noexcept : pool_(&pool) {}

void WorkerPoolExecutor::Submit(Job job) {
    // After Stop the pool takes nothing and returns false: the job is dropped, as the executor contract allows.
    (void)pool_->Submit(std::move(job));
}

}  // namespace GYO::Engine::WorldAsset
