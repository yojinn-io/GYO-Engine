#include "TestSupport.hpp"

#include <array>
#include <exception>
#include <iostream>
#include <string>

namespace {

struct TestSuite {
    const char* name;
    void (*run)(fps::tests::TestContext&);
};

} // namespace

int main() {
    fps::tests::TestContext context;
    constexpr std::array<TestSuite, 17> suites = {{
        {"World", fps::tests::RunWorldTests},
        {"Collision", fps::tests::RunCollisionTests},
        {"Collision.Combat", fps::tests::RunCombatCollisionTests},
        {"Rendering.MapGeometry", fps::tests::RunMapGeometryTests},
        {"Rendering.EnemyBillboard", fps::tests::RunEnemyBillboardTests},
        {"Gameplay.PlayerController", fps::tests::RunPlayerControllerTests},
        {"Gameplay.PlayerCombatState", fps::tests::RunPlayerCombatStateTests},
        {"Gameplay.WeaponController", fps::tests::RunWeaponControllerTests},
        {"Gameplay.ProjectileSystem", fps::tests::RunProjectileSystemTests},
        {"Gameplay.EnemySystem", fps::tests::RunEnemySystemTests},
        {"Gameplay.EnemySpawnDirector", fps::tests::RunEnemySpawnDirectorTests},
        {"Data.GameDataCatalog", fps::tests::RunGameDataCatalogTests},
        {"Game.Flow", fps::tests::RunGameFlowTests},
        {"Game.CampaignRunState", fps::tests::RunCampaignRunStateTests},
        {"Game.Session", fps::tests::RunGameSessionTests},
        {"App.ObjectFpsUi", fps::tests::RunObjectFpsUiTests},
        {"App.ObjectFpsPresentation", fps::tests::RunObjectFpsPresentationTests},
    }};

    for (const TestSuite& suite : suites) {
        try {
            suite.run(context);
        } catch (const std::exception& exception) {
            context.Fail(std::string{"Unhandled exception in "} + suite.name + ": " +
                         exception.what());
        } catch (...) {
            context.Fail(std::string{"Unhandled unknown exception in "} + suite.name);
        }
    }

    if (context.GetFailureCount() != 0) {
        std::cerr << context.GetFailureCount() << " core test(s) failed.\n";
        return 1;
    }

    std::cout << "All 17 core suites passed (" << context.GetAssertionCount()
              << " assertions).\n";
    return 0;
}
