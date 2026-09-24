#include "RetroFPS/Pvp/IpcHost.hpp"
#include "gyo/AppConfig.hpp"
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace {
volatile std::sig_atomic_t stopping=0;
void Stop(int){stopping=1;}

// Product executable composition only; no window backend or Engine dependency.
std::filesystem::path ExecutablePath() {
#if defined(_WIN32)
    std::wstring buffer(256, L'\0');
    for (;;) {
        const DWORD length=GetModuleFileNameW(nullptr,buffer.data(),static_cast<DWORD>(buffer.size()));
        if(length==0) throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),
                                             "Cannot locate Match executable");
        if(length<buffer.size()) {
            buffer.resize(length);
            return std::filesystem::path(buffer);
        }
        if(buffer.size()>=32768) throw std::runtime_error("Match executable path is too long");
        buffer.resize(buffer.size()*2);
    }
#elif defined(__linux__)
    return std::filesystem::read_symlink("/proc/self/exe");
#elif defined(__APPLE__)
    std::vector<char> buffer(1024);
    std::uint32_t size=static_cast<std::uint32_t>(buffer.size());
    if(_NSGetExecutablePath(buffer.data(),&size)!=0) {
        buffer.resize(size);
        if(_NSGetExecutablePath(buffer.data(),&size)!=0)
            throw std::runtime_error("Cannot locate Match executable");
    }
    return std::filesystem::canonical(buffer.data());
#else
    throw std::runtime_error("Cannot locate Match executable on this platform; use --arena");
#endif
}
}

int main(int argc,char** argv) {
    try {
        std::string listen="127.0.0.1:27016";
        std::optional<std::filesystem::path> explicitArena;
        for(int index=1;index<argc;++index) {
            const std::string argument=argv[index];
            if(argument=="--help") {
                std::cout<<"Match runtime: --arena path --listen 127.0.0.1:27016\n";return 0;
            }
            if(index+1>=argc){std::cerr<<"Missing argument\n";return 2;}
            if(argument=="--arena") explicitArena=argv[++index];
            else if(argument=="--listen") listen=argv[++index];
            else {std::cerr<<"Unknown argument: "<<argument<<'\n';return 2;}
        }
        const auto arenaPath=explicitArena ? *explicitArena
            : ExecutablePath().parent_path()/Gyo::AppConfig::Assets/"pvp_arena.json";
        std::string error;
        auto arena=fps::pvp::Arena::Load(arenaPath,error);
        if(!arena){std::cerr<<error<<'\n';return 1;}
        fps::pvp::MatchRuntimeHost runtime(*arena);
        std::jthread simulation([&](std::stop_token stop){runtime.Run(stop);});
        fps::pvp::IpcHost ipc(runtime,*arena);
        if(!ipc.Start(listen,error)){std::cerr<<error<<'\n';return 1;}
        std::signal(SIGINT,Stop);std::signal(SIGTERM,Stop);
        std::cout<<"Object_FPS_PVP Match ready: "<<listen<<" arena="<<arena->id<<" authority=60Hz\n"<<std::flush;
        while(!stopping) std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ipc.Stop(); simulation.request_stop();
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
    return 0;
}
