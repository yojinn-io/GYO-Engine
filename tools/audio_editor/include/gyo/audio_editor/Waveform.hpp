#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace Gyo::Tools::AudioEditor {

// The extremes of the samples one column of a waveform covers.
struct WaveColumn final {
    double min{};
    double max{};

    friend bool operator==(const WaveColumn&, const WaveColumn&) = default;
};

// columns extremes of samples: column c covers the samples from
// floor(c * n / columns) up to floor((c + 1) * n / columns), and at least the
// one it starts on, so a range shorter than the columns repeats samples.
// Empty samples give no columns.
[[nodiscard]] std::vector<WaveColumn> Waveform(std::span<const double> samples, std::size_t columns);

} // namespace Gyo::Tools::AudioEditor
