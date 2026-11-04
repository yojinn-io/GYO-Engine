#include "gyo/audio_editor/Waveform.hpp"

#include <algorithm>

namespace Gyo::Tools::AudioEditor {

std::vector<WaveColumn> Waveform(const std::span<const double> samples, const std::size_t columns) {
    std::vector<WaveColumn> result;
    if (samples.empty() || columns == 0) return result;
    const std::size_t count = samples.size();
    result.reserve(columns);
    for (std::size_t column = 0; column < columns; ++column) {
        const std::size_t first = column * count / columns;
        const std::size_t last = std::max(first + 1, (column + 1) * count / columns);
        const auto [low, high] = std::minmax_element(samples.begin() + static_cast<std::ptrdiff_t>(first),
                                                     samples.begin() + static_cast<std::ptrdiff_t>(last));
        result.push_back({*low, *high});
    }
    return result;
}

} // namespace Gyo::Tools::AudioEditor
