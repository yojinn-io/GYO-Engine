// Criterion 7 of the declaration (a stop condition, not a gate): the median
// time of Shape + Layout for a Japanese string of about 12 characters at
// 24 px, the atlas already warm. Built on demand (not part of ALL, not a
// test): cmake --build <dir> --target gyo_text_measure_frame_cost
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "engine/text/TextContext.hpp"

using namespace GYO::Engine;

int main() {
    const std::filesystem::path path = std::filesystem::path(GYO_TEXT_TEST_FONTS) / "BIZUDPGothic-Subset.ttf";
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> chars((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<std::byte> bytes(chars.size());
    std::transform(chars.begin(), chars.end(), bytes.begin(), [](const char c) { return static_cast<std::byte>(c); });
    Text::TextContext context;
    const Text::FontId id = context.AddFont(Text::FontData::FromBytes(std::move(bytes)).value(), 0).value();
    const std::vector<Text::FontId> fonts{id};
    Text::GlyphAtlas atlas;
    const std::vector<std::string> texts{"残り 3 / 12 ステージ", "残り 7 / 12 ステージ", "ゲームパッドで決定"};
    std::vector<double> micros;
    for (int round = 0; round < 20000; ++round) {
        const std::string& text = texts[static_cast<std::size_t>(round) % texts.size()];
        const Text::TextSpan span{0, static_cast<std::uint32_t>(text.size()), Text::TextStyle{fonts, 24.0f, {}}};
        const auto start = std::chrono::steady_clock::now();
        const Text::TextLayout layout =
            context.Layout(context.Shape(text, {&span, 1}), Text::LayoutOptions{300.0f, Text::HorizontalAlign::Center});
        const auto stop = std::chrono::steady_clock::now();
        static_cast<void>(context.Rasterize(layout, atlas));
        if (round >= 1000) {
            micros.push_back(std::chrono::duration<double, std::micro>(stop - start).count());
        }
    }
    std::sort(micros.begin(), micros.end());
    std::printf("samples %zu median %.2f us p90 %.2f us p99 %.2f us\n", micros.size(), micros[micros.size() / 2],
                micros[micros.size() * 9 / 10], micros[micros.size() * 99 / 100]);
    return 0;
}
