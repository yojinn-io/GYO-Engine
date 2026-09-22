#pragma once
#include "RetroFPS/App/ObjectFpsApplication.hpp"
#include "RetroFPS/Game/GameSession.hpp"
#include "gyo/AppConfig.hpp"
#include <SDL3/SDL_filesystem.h>
#include <array>
#include <filesystem>
#include <stdexcept>

namespace fps::tests {
inline ObjectFpsApplication& ProductionApplication() {
    // Each owner test runs from its assembled product; no source-tree fallback
    // or reference to another game's content is allowed here.
    static auto application=[] {
        auto app=std::make_unique<ObjectFpsApplication>();
        const char* base=SDL_GetBasePath();
        if(!base) throw std::runtime_error("Cannot locate test product directory");
        std::string error;
        if(!app->InitializeContent(std::filesystem::path(base)/Gyo::AppConfig::Assets,error))
            throw std::runtime_error(error);
        return app;
    }();
    return *application;
}
inline GameSession StartedSession() {
    GameSession session;
    GameSessionConfig config;
    config.fadeInSeconds=0.001F;config.fadeOutSeconds=0.001F;
    std::string error;
    if(!session.Initialize(ProductionApplication().Content(),config,error)) throw std::runtime_error(error);
    const std::array<GameSessionCommand,1> commands{StartCampaignCommand{}};
    if(!session.Advance(0.016F,{},commands,error)) throw std::runtime_error(error);
    for(int i=0;i<4;++i) if(!session.Advance(0.016F,{},{},error)) throw std::runtime_error(error);
    return session;
}
}
