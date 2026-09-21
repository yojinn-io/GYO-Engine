# Sources belonging to the optional quality suite.

set(APP_HEADLESS_SOURCES
    tests/TestMain.cpp
    tests/App/AssetPresentationDefinitionTests.cpp
    tests/App/ObjectFpsPresentationTests.cpp
    tests/App/ObjectFpsUiTests.cpp
    tests/Collision/CollisionTests.cpp
    tests/Collision/CombatCollisionTests.cpp
    tests/Data/GameDataCatalogTests.cpp
    tests/Game/CampaignRunStateTests.cpp
    tests/Game/GameFlowTests.cpp
    tests/Game/GameSessionTests.cpp
    tests/Gameplay/EnemySpawnDirectorTests.cpp
    tests/Gameplay/EnemySystemTests.cpp
    tests/Gameplay/PlayerCombatStateTests.cpp
    tests/Gameplay/PlayerControllerTests.cpp
    tests/Gameplay/ProjectileSystemTests.cpp
    tests/Gameplay/WeaponControllerTests.cpp
    tests/Rendering/EnemyBillboardTests.cpp
    tests/Rendering/MapGeometryTests.cpp
    tests/World/WorldTests.cpp
)

set(APP_HEADLESS_SUPPORT_SOURCES
    src/App/AnimationSetDefinition.cpp
    src/App/CharacterPresentationDefinition.cpp
    src/App/ObjectFpsPresentation.cpp
    src/App/WeaponViewModel.cpp
    src/App/WeaponPresentationDefinition.cpp
    src/App/CampaignContentLoader.cpp
)
