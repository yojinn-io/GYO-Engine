#pragma once

#include "object_connect/data/PuzzleCatalogLoader.hpp"
#include "object_connect/data/PuzzleData.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace object_connect::tests {

class TestContext final {
public:
    void Expect(const bool condition, const std::string_view description) {
        if (condition) {
            return;
        }

        std::cerr << "FAILED: " << description << '\n';
        ++failureCount_;
    }

    void Fail(const std::string_view description) { Expect(false, description); }

    template <typename Exception, typename Callable>
    void ExpectThrows(Callable&& callable, const std::string_view description) {
        try {
            std::forward<Callable>(callable)();
        } catch (const Exception&) {
            return;
        } catch (...) {
            Fail(description);
            return;
        }

        Fail(description);
    }

    [[nodiscard]] int GetFailureCount() const noexcept { return failureCount_; }

private:
    int failureCount_ = 0;
};

[[nodiscard]] inline bool NearlyEqual(const float left, const float right,
                                      const float tolerance = 0.0001f) noexcept {
    return std::fabs(left - right) <= tolerance;
}

// The bundled level data under assets/object_connect (BundledData.cpp).
[[nodiscard]] bool LoadBundledCatalog(PuzzleCatalog& catalog, std::string& error);
[[nodiscard]] bool LoadBundledNodePresets(NodePresetCatalog& catalog, std::string& error);

void RunPuzzleCatalogTests(TestContext& context);
void RunFinalResultsTests(TestContext& context);
void RunGameFlowTests(TestContext& context);
void RunGeometry2DTests(TestContext& context);
void RunMouseCursorStateTests(TestContext& context);
void RunMusicSequencePolicyTests(TestContext& context);
void RunPuzzleBoardTests(TestContext& context);
void RunBloodTentacleTests(TestContext& context);
void RunRibbonStripTests(TestContext& context);

} // namespace object_connect::tests
