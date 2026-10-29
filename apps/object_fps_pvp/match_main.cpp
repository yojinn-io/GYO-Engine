#include "RetroFPS/Pvp/ClogLine.hpp"
#include "RetroFPS/Pvp/IngressStatistics.hpp"
#include "RetroFPS/Pvp/IpcHost.hpp"
#include "RetroFPS/Pvp/LogFile.hpp"
#include "RetroFPS/Pvp/MatchIngressTrace.hpp"
#include "RetroFPS/Pvp/MatchShotTrace.hpp"
#include "RetroFPS/Pvp/MovementTraceWriter.hpp"
#include "RetroFPS/Pvp/NetworkStatistics.hpp"
#include "RetroFPS/Pvp/TracePaths.hpp"
#include "engine/os/Console.hpp"
#include "engine/os/Files.hpp"
#include "gyo/AppConfig.hpp"
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <vector>

namespace {
std::uint64_t WindowMillis(std::chrono::steady_clock::duration window) {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(window).count());
}
// One window's ingress lines, written together so no other log line splits them.
void WriteIngress(const std::vector<std::string>& lines) {
    fps::pvp::ClogLine out;
    for(const auto& line:lines) out<<line<<'\n';
}
}

int main(int argc,char** argv) {
    const auto processStarted=std::chrono::steady_clock::now();
    // UTF-8 shows as UTF-8 on a Windows console until main returns.
    const auto utf8Console=Engine::Os::Console::Utf8ConsoleOutput::Enable();
    try {
        std::string listen="127.0.0.1:27016";
        std::optional<std::filesystem::path> explicitArena;
        std::filesystem::path movementTrace,ingressTraceOverride,logPath;
        // UTF-8 arguments on every platform (the narrow argv is the ANSI code page on Windows).
        const auto commandLine=Engine::Os::Console::CommandLineUtf8(argc,argv);
        if(!commandLine){std::cerr<<Engine::Base::Describe(commandLine.error())<<'\n';return 2;}
        const auto& arguments=*commandLine;
        std::string invalidPath;
        const auto pathArgument=[&invalidPath](const std::string& text) {
            auto path=Engine::Os::Files::PathFromUtf8(text);
            if(!path){invalidPath=Engine::Base::Describe(path.error());return std::filesystem::path{};}
            return std::move(*path);
        };
        for(std::size_t index=1;index<arguments.size();++index) {
            const std::string& argument=arguments[index];
            if(argument=="--help") {
                std::cout<<"Match runtime: [--arena path (default: the release arena pvp_corners.json)] [--listen 127.0.0.1:27016]\n"
                           "  [--movement-trace path [--ingress-trace path (default: next to it, -commands.jsonl -> -ingress.jsonl)]]\n"
                           "  (with a movement trace, the shadow records go next to it: -commands.jsonl -> -shots.jsonl)\n"
                           "  [--log path (also append console output, timestamped)]\n";return 0;
            }
            if(index+1>=arguments.size()){std::cerr<<"Missing argument\n";return 2;}
            if(argument=="--arena") explicitArena=pathArgument(arguments[++index]);
            else if(argument=="--listen") listen=arguments[++index];
            else if(argument=="--movement-trace") movementTrace=pathArgument(arguments[++index]);
            else if(argument=="--ingress-trace") ingressTraceOverride=pathArgument(arguments[++index]);
            else if(argument=="--log") logPath=pathArgument(arguments[++index]);
            else {std::cerr<<"Unknown argument: "<<argument<<'\n';return 2;}
            if(!invalidPath.empty()){std::cerr<<"Invalid path for "<<argument<<": "<<invalidPath<<'\n';return 2;}
        }
        // A restart never truncates an earlier run's files (D191): the movement
        // trace takes the next free name whose ingress and shot files are free too.
        movementTrace=fps::pvp::FreshTracePath(movementTrace,[&](const std::filesystem::path& candidate) {
            std::string ignored;
            return std::vector<std::filesystem::path>{
                ingressTraceOverride.empty()?fps::pvp::MatchIngressTracePath(candidate,{},ignored):std::filesystem::path{},
                fps::pvp::MatchShotTracePath(candidate)};
        });
        ingressTraceOverride=fps::pvp::FreshTracePath(ingressTraceOverride);
        // The ingress detail file (match-ingress.jsonl) is written only with a movement trace.
        std::string usage;
        const auto ingressTracePath=fps::pvp::MatchIngressTracePath(movementTrace,ingressTraceOverride,usage);
        if(!usage.empty()){std::cerr<<usage<<'\n';return 2;}
        std::optional<fps::pvp::LogFile> log;
        if(!logPath.empty()) {log.emplace(logPath);log->FlushOnCrash();log->Tee(std::cout);log->Tee(std::cerr);log->Tee(std::clog);}
        // Without --arena the Match hosts the release arena; tests and acceptance name theirs.
        const auto arenaPath=explicitArena ? *explicitArena
            : fps::pvp::RunningExecutablePath().parent_path()/Gyo::AppConfig::Assets/"pvp_corners.json";
        std::clog<<"[ObjectFPS/PvP Match] start executable_sha256="<<fps::pvp::FileSha256(fps::pvp::RunningExecutablePath())
                 <<" arena_file="<<Engine::Os::Files::PathForMessage(arenaPath)<<'\n';
        std::string error;
        auto arena=fps::pvp::Arena::Load(arenaPath,error);
        if(!arena){std::cerr<<error<<'\n';return 1;}
        std::clog<<"[ObjectFPS/PvP Match] arena id="<<arena->id<<" version="<<arena->version<<" spawns="<<arena->spawns.size()<<'\n';
        if(!movementTrace.empty())
            std::clog<<"[ObjectFPS/PvP Match] trace files movement="<<Engine::Os::Files::PathForMessage(movementTrace)
                     <<" ingress="<<Engine::Os::Files::PathForMessage(ingressTracePath)
                     <<" shots="<<Engine::Os::Files::PathForMessage(fps::pvp::MatchShotTracePath(movementTrace))<<'\n';
        fps::pvp::MovementTraceWriter trace(movementTrace);
        std::optional<fps::pvp::MatchIngressTraceFile> ingressTrace;
        if(!ingressTracePath.empty()) ingressTrace.emplace(ingressTracePath);
        std::optional<fps::pvp::MatchShotTraceFile> shotTrace;
        if(const auto path=fps::pvp::MatchShotTracePath(movementTrace);!path.empty()) shotTrace.emplace(path);
        fps::pvp::MatchRuntimeHost runtime(*arena);
        fps::pvp::IpcHost ipc(runtime,*arena);
        if(!runtime.Start(error)){std::cerr<<error<<'\n';return 1;}
        if(!ipc.Start(listen,error)){std::cerr<<error<<'\n';return 1;}
        // SIGINT and SIGTERM, and on Windows Ctrl+C, Ctrl+Break and closing the console, end the loop below.
        if(auto installed=Engine::Os::Console::InstallShutdownRequests();!installed)
            std::cerr<<"[ObjectFPS/PvP Match] shutdown requests unavailable: "<<Engine::Base::Describe(installed.error())<<'\n';
        std::cout<<"Object_FPS_PVP Match ready: "<<listen<<" arena="<<arena->id<<" authority=60Hz\n"<<std::flush;
        // Diagnostics: one statistics line per window, written here so that the
        // tick and IPC threads do no logging I/O for it. The IPC thread logs too,
        // so this line is emitted through ClogLine as well.
        auto windowStart=std::chrono::steady_clock::now();
        auto cpuAtStart=fps::pvp::ProcessCpuSeconds();
        auto ipcAtStart=ipc.Iterations();
        static_cast<void>(runtime.TakeStatistics());
        // Ingress lines share the window; nothing is taken before the first one.
        fps::pvp::MatchIngressStatisticsWriter ingress;
        fps::pvp::MatchResyncStatisticsWriter resync;
        std::optional<std::string> failed;
        while(!Engine::Os::Console::RequestedShutdown() && !(failed=runtime.Error())) {
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
            fps::pvp::ClogLine()<<fps::pvp::MatchStatisticsLine({.windowSeconds=window,
                .cpuSeconds=cpu && cpuAtStart ? std::optional<double>(*cpu-*cpuAtStart) : std::nullopt,
                .cpuTotalSeconds=cpu,.ipcIterations=iterations-ipcAtStart,.ticks=std::move(host.ticks),
                .snapshotOverwrites=host.snapshotOverwrites,.lockWaits=std::move(host.lockWaits),.pace=std::move(host.pace)})<<'\n';
            fps::pvp::ClogLine()<<fps::pvp::IpcStatisticsLine(ipc.TakeLaneStatistics(),window,false)<<'\n';
            WriteIngress(resync.Lines(taken,false));
            WriteIngress(ingress.Lines(std::move(taken),WindowMillis(now-windowStart),false));
            // The detail file follows the same window; a write failure exits 1 at the end.
            if(ingressTrace) ingressTrace->Write(records);
            const auto shots=runtime.DrainShotRecords();
            if(shotTrace) shotTrace->Write(shots);
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
        fps::pvp::ClogLine()<<fps::pvp::IpcStatisticsLine(ipc.TakeLaneStatistics(),
            std::chrono::duration<double>(stoppedAt-windowStart).count(),true)<<'\n';
        WriteIngress(resync.Lines(lastTaken,true));
        WriteIngress(ingress.Lines(std::move(lastTaken),WindowMillis(stoppedAt-windowStart),true));
        trace.Finish();
        if(failed) std::cerr<<"Match simulation failed: "<<*failed<<'\n';
        std::string reason=failed ? "runtime_failed" : "signal";
        int code=failed ? 1 : 0;
        try {
            // Ends the file with trace_end; throws (exit 1) if any of its writes failed.
            if(ingressTrace) ingressTrace->Finish(lastRecords);
            if(shotTrace) shotTrace->Finish(runtime.DrainShotRecords());
            if(!trace.Good()) throw std::runtime_error("Movement trace lost diagnostic data");
        } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';reason="exception";code=1;}
        // log-contract: the Match's own end record, the last line (none when the process is killed,
        // e.g. by TerminateProcess on Windows).
        fps::pvp::ClogLine()<<"[ObjectFPS/PvP Match] end version=1 reason="<<reason<<" exit_code="<<code
            <<" uptime_s="<<std::fixed<<std::setprecision(3)
            <<std::chrono::duration<double>(std::chrono::steady_clock::now()-processStarted).count()
            <<" authority_tick="<<runtime.TickCount()<<'\n';
        return code;
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
