#pragma once

#include <cmath>
#include <iostream>
#include <string_view>
#include <utility>

namespace fps::tests {

class TestContext final {
public:
    void Expect(const bool condition, const std::string_view description) {
        ++assertionCount_;
        if (condition) {
            return;
        }

        std::cerr << "FAILED: " << description << '\n';
        ++failureCount_;
    }

    void Fail(const std::string_view description) { Expect(false, description); }

    template <typename Exception, typename Callable>
    void ExpectThrows(Callable&& callable, const std::string_view description) {
        ++assertionCount_;
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
    [[nodiscard]] int GetAssertionCount() const noexcept { return assertionCount_; }

private:
    int assertionCount_ = 0;
    int failureCount_ = 0;
};

[[nodiscard]] inline bool NearlyEqual(const float left, const float right,
                                      const float tolerance = 0.0001f) noexcept {
    return std::fabs(left - right) <= tolerance;
}

void RunWorldTests(TestContext& context);
void RunCollisionTests(TestContext& context);
void RunCombatCollisionTests(TestContext& context);
void RunMapGeometryTests(TestContext& context);
void RunEnemyBillboardTests(TestContext& context);
void RunPlayerControllerTests(TestContext& context);
void RunPlayerCombatStateTests(TestContext& context);
void RunWeaponControllerTests(TestContext& context);
void RunProjectileSystemTests(TestContext& context);
void RunEnemySystemTests(TestContext& context);
void RunEnemySpawnDirectorTests(TestContext& context);
void RunGameDataCatalogTests(TestContext& context);
void RunGameFlowTests(TestContext& context);
void RunCampaignRunStateTests(TestContext& context);
void RunGameSessionTests(TestContext& context);
void RunObjectFpsUiTests(TestContext& context);
void RunObjectFpsPresentationTests(TestContext& context);

} // namespace fps::tests
