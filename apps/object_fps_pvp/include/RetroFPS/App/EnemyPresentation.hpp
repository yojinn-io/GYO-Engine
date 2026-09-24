#pragma once
#include "RetroFPS/Game/GameSession.hpp"
#include <memory>
namespace Engine::Asset {
class AssetManager;
}
namespace Engine::Render {
class IRenderDevice;
class RenderQueue;
} // namespace Engine::Render
namespace fps {
class EnemyPresentation final {
  public:
    EnemyPresentation();
    ~EnemyPresentation();
    bool Initialize(Engine::Render::IRenderDevice&, Engine::Asset::AssetManager&,
                    const EnemyCatalog&, std::string& error);
    bool Submit(const GameSessionSnapshot&, Engine::Render::RenderQueue&, bool showCollisionVolumes,
                std::string& error);
    void Reset() noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace fps
