#pragma once
// The simulation role's deadline wakes over a probe run as one JSON object: loop
// runs, how many deadline wakes were late, the latest, the upper bounds of their
// median and 99th percentile, and the fixed bins. Interpretation only (v7 batch 04):
// no analyzer rule reads it.
#include "RetroFPS/Pvp/ClientSimulationRole.hpp"

#include <chrono>
#include <sstream>
#include <string>

inline std::string SimulationWakesJson(const fps::pvp::ClientSimulationRole::WakeReport& report) {
    const auto micros = [](Engine::Time::Duration value) {
        return std::chrono::duration_cast<std::chrono::microseconds>(value).count();
    };
    const auto& late = report.late;
    std::ostringstream out;
    out << "{\"runs\":" << report.runs << ",\"deadline_wakes\":" << late.Count()
        << ",\"late_max_us\":" << micros(late.Max()) << ",\"late_p50_upper_us\":" << micros(late.QuantileUpperBound(.5))
        << ",\"late_p99_upper_us\":" << micros(late.QuantileUpperBound(.99)) << ",\"bin_upper_us\":[";
    for (std::size_t index = 0; index < Engine::Time::LateWakeStats::BinUpperMicros.size(); ++index)
        out << (index ? "," : "") << Engine::Time::LateWakeStats::BinUpperMicros[index];
    out << "],\"bins\":[";
    for (std::size_t index = 0; index < late.Bins().size(); ++index) out << (index ? "," : "") << late.Bins()[index];
    out << "]}";
    return out.str();
}
