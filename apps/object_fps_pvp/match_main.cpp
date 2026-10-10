#include "RetroFPS/Pvp/IngressStatistics.hpp"
#include "RetroFPS/Pvp/IpcHost.hpp"
#include "RetroFPS/Pvp/LogFile.hpp"
#include "RetroFPS/Pvp/MatchIngressTrace.hpp"
#include "RetroFPS/Pvp/MovementTraceWriter.hpp"
#include "RetroFPS/Pvp/NetworkStatistics.hpp"
#include "gyo/AppConfig.hpp"
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <syncstream>
#include <system_error>
#include <thread>
#include <vector>

namespace {
volatile std::sig_atomic_t stopping=0;
void Stop(int){stopping=1;}

std::uint64_t WindowMillis(std::chrono::steady_clock::duration window) {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(window).count());
}
// One window's ingress lines, written together so no other log line splits them.
void WriteIngress(const std::vector<std::string>& lines) {
    std::osyncstream out(std::clog);
    for(const auto& line:lines) out<<line<<'\n';
}
}

int main(int argc,char** argv) {
    try {
        std::string listen="127.0.0.1:27016";
        std::optional<std::filesystem::path> explicitArena;
        std::filesystem::path movementTrace,ingressTraceOverride,logPath;
        for(int index=1;index<argc;++index) {
            const std::string argument=argv[index];
            if(argument=="--help") {
                std::cout<<"Match runtime: [--arena path (default: the release arena pvp_corners.json)] [--listen 127.0.0.1:27016]\n"
                           "  [--movement-trace path [--ingress-trace path (default: next to it, -commands.jsonl -> -ingress.jsonl)]]\n"
                           "  [--log path (also append console output, timestamped)]\n";return 0;
            }
            if(index+1>=argc){std::cerr<<"Missing argument\n";return 2;}
            if(argument=="--arena") explicitArena=argv[++index];
            else if(argument=="--listen") listen=argv[++index];
            else if(argument=="--movement-trace") movementTrace=argv[++index];
            else if(argument=="--ingress-trace") ingressTraceOverride=argv[++index];
            else if(argument=="--log") logPath=argv[++index];
            else {std::cerr<<"Unknown argument: "<<argument<<'\n';return 2;}
        }
        // The ingress detail file (match-ingress.jsonl) is written only with a movement trace.
        std::string usage;
        const auto ingressTracePath=fps::pvp::MatchIngressTracePath(movementTrace,ingressTraceOverride,usage);
        if(!usage.empty()){std::cerr<<usage<<'\n';return 2;}
        std::optional<fps::pvp::LogFile> log;
        if(!logPath.empty()) {log.emplace(logPath);log->Tee(std::cout);log->Tee(std::cerr);log->Tee(std::clog);}
        // Without --arena the Match hosts the release arena; tests and acceptance name theirs.
        const auto arenaPath=explicitArena ? *explicitArena
            : fps::pvp::RunningExecutablePath().parent_path()/Gyo::AppConfig::Assets/"pvp_corners.json";
        std::clog<<"[ObjectFPS/PvP Match] start executable_sha256="<<fps::pvp::FileSha256(fps::pvp::RunningExecutablePath())
                 <<" arena_file="<<arenaPath.string()<<'\n';
        std::string error;
        auto arena=fps::pvp::Arena::Load(arenaPath,error);
        if(!arena){std::cerr<<error<<'\n';return 1;}
        std::clog<<"[ObjectFPS/PvP Match] arena id="<<arena->id<<" version="<<arena->version<<" spawns="<<arena->spawns.size()<<'\n';
        fps::pvp::MovementTraceWriter trace(movementTrace);
        std::optional<fps::pvp::MatchIngressTraceFile> ingressTrace;
        if(!ingressTracePath.empty()) ingressTrace.emplace(ingressTracePath);
        fps::pvp::MatchRuntimeHost runtime(*arena);
        fps::pvp::IpcHost ipc(runtime,*arena);
        if(!runtime.Start(error)){std::cerr<<error<<'\n';return 1;}
        if(!ipc.Start(listen,error)){std::cerr<<error<<'\n';return 1;}
        std::signal(SIGINT,Stop);std::signal(SIGTERM,Stop);
        std::cout<<"Object_FPS_PVP Match ready: "<<listen<<" arena="<<arena->id<<" authority=60Hz\n"<<std::flush;
        // Diagnostics: one statistics line per window, written here so that the
        // tick and IPC threads do no logging I/O for it. The IPC thread logs too,
        // so this line is emitted through osyncstream as well.
        auto windowStart=std::chrono::steady_clock::now();
        auto cpuAtStart=fps::pvp::ProcessCpuSeconds();
        auto ipcAtStart=ipc.Iterations();
        static_cast<void>(runtime.TakeStatistics());
        // Ingress lines share the window; nothing is taken before the first one.
        fps::pvp::MatchIngressStatisticsWriter ingress;
        std::optional<std::string> failed;
        while(!stopping && !(failed=runtime.Error())) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            const auto now=std::chrono::steady_clock::now();
            const double window=std::chrono::duration<double>(now-windowStart).count();
            if(window<fps::pvp::NetworkStatisticsSeconds) continue;
            const auto cpu=fps::pvp::ProcessCpuSeconds();
            const auto iterations=ipc.Iterations();
            auto host=runtime.TakeStatistics();
            // Taken next to `now`, before any line is written, so window_ms is its window.
            auto taken=runtime.TakeIngressStatistics();
            auto records=runtime.DrainIngressRecords();
            std::osyncstream(std::clog)<<fps::pvp::MatchStatisticsLine({.windowSeconds=window,
                .cpuSeconds=cpu && cpuAtStart ? std::optional<double>(*cpu-*cpuAtStart) : std::nullopt,
                .cpuTotalSeconds=cpu,.ipcIterations=iterations-ipcAtStart,.ticks=std::move(host.ticks),
                .snapshotOverwrites=host.snapshotOverwrites})<<'\n';
            WriteIngress(ingress.Lines(std::move(taken),WindowMillis(now-windowStart),false));
            // The detail file follows the same window; a write failure exits 1 at the end.
            if(ingressTrace) ingressTrace->Write(records);
            windowStart=now;cpuAtStart=cpu;ipcAtStart=iterations;
        }
        // I/O first: it stops calling into the host before the host's role stops.
        ipc.Stop(); runtime.Stop();
        // The last ingress window: what the stopped IPC and host counted since the previous line.
        const auto stoppedAt=std::chrono::steady_clock::now();
        // The final take closes the open substitution records as end; draining
        // after it puts them in the detail file before its trace_end.
        auto lastTaken=runtime.TakeIngressStatistics(true);
        const auto lastRecords=runtime.DrainIngressRecords();
        WriteIngress(ingress.Lines(std::move(lastTaken),WindowMillis(stoppedAt-windowStart),true));
        trace.Finish();
        if(failed) std::cerr<<"Match simulation failed: "<<*failed<<'\n';
        // Ends the file with trace_end; throws (exit 1) if any of its writes failed.
        if(ingressTrace) ingressTrace->Finish(lastRecords);
        if(failed) return 1;
        if(!trace.Good()) throw std::runtime_error("Movement trace lost diagnostic data");
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
    return 0;
}
