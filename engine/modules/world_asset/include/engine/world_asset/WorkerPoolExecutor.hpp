#pragma once

#include "engine/asset/AssetExecutor.hpp"
#include "engine/threads/WorkerPool.hpp"

namespace GYO::Engine::WorldAsset {

// threads' WorkerPool behind asset's AssetExecutor (declaration
// docs/architecture/target/v2/asset-declaration.zh-Hant.md, section 15 item 13). It only forwards: it starts no
// thread and owns no pool. The product creates the pool, chooses how many threads it has, keeps it alive longer
// than the Product, and passes this through AssetOptions::executor; a product may give any other AssetExecutor
// instead. WorkerPool::Submit queues a job submitted from inside a job and takes nothing after Stop, which is what
// the executor contract asks.
class WorkerPoolExecutor final : public Asset::AssetExecutor {
public:
    explicit WorkerPoolExecutor(Threads::WorkerPool& pool) noexcept;

    void Submit(Job job) override;

private:
    Threads::WorkerPool* pool_;
};

}  // namespace GYO::Engine::WorldAsset
