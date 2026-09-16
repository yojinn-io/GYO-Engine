#include "../TestSupport.hpp"

#include "RetroFPS/Data/Csv.hpp"
#include "RetroFPS/Data/GameData.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>

namespace fps::tests {
namespace {

constexpr std::string_view kValidEnemies =
    "enemy_id,kind,damage,attack_interval_seconds,hp,defense,hitbox_radius,hitbox_height,render_width,render_height,texture_asset_id,frame_width_px,frame_height_px\n"
    "melee_basic,melee,15,0.9,50,5,0.2,0.8,0.973913,0.8,object_fps.texture.enemy.blood_dog,560,460\n"
    "ranged_basic,ranged,10,1.25,40,0,0.2,1.6,1.230769,1.6,object_fps.texture.enemy.spitter,700,910\n";

constexpr std::string_view kValidEnemyAnimations =
    "enemy_id,state,origin_x_px,origin_y_px,frame_count,seconds_per_frame,event_frame_index,muzzle_x_px,muzzle_y_px\n"
    "melee_basic,idle,0,0,3,0.1,,,\n"
    "melee_basic,move,0,460,4,0.1,,,\n"
    "melee_basic,attack,0,920,6,0.05,3,,\n"
    "melee_basic,dead,0,1380,4,0.1,,,\n"
    "ranged_basic,idle,0,0,3,0.1,,,\n"
    "ranged_basic,move,0,910,4,0.1,,,\n"
    "ranged_basic,attack,0,1820,5,0.05,2,350,420\n"
    "ranged_basic,dead,0,2730,4,0.1,,,\n";

constexpr std::string_view kValidWeapons =
    "weapon_id,damage,magazine_size,reserve_ammo,recoil,automatic,fire_interval_seconds,reload_seconds,draw_seconds,hide_seconds,presentation_asset_id\n"
    "starter_pistol,25,12,48,1.5,false,0.3333333333,3.7333333333,0.8333333333,0.3666666667,object_fps.weapon.mark23\n";

[[nodiscard]] std::string MakeLevels(const std::size_t count) {
    std::ostringstream csv;
    csv << "level_id,level_name,map_asset_id,next_level_id,ranged_enemy_count,"
           "melee_enemy_count,active_enemy_limit,clear_kill_count\n";
    for (std::size_t index = 0; index < count; ++index) {
        csv << "room_" << index << ",ROOM " << index << ",object_fps.map.room_"
            << index << ',';
        if (index + 1 < count) {
            csv << "room_" << (index + 1);
        }
        csv << ",2,2,2,3\n";
    }
    return csv.str();
}

[[nodiscard]] GameDataLoadResult ParseWith(
    const std::string_view enemies,
    const std::string_view enemyAnimations,
    const std::string_view weapons,
    const std::string_view levels) {
    return GameDataLoader::Parse(enemies, enemyAnimations, weapons, levels);
}

[[nodiscard]] GameDataLoadResult ParseWith(const std::string_view levels) {
    return ParseWith(kValidEnemies, kValidEnemyAnimations, kValidWeapons, levels);
}

void ExpectRejected(
    TestContext& context,
    const GameDataLoadResult& result,
    const std::string_view expectedErrorFragment,
    const std::string_view description) {
    context.Expect(!result.Succeeded(), description);
    context.Expect(!result.catalog.has_value(), "rejected data has no partial catalog");
    context.Expect(
        result.error.find(expectedErrorFragment) != std::string::npos,
        "rejected data reports a useful diagnostic category");
}

[[nodiscard]] std::string ReplaceOnce(
    const std::string_view source,
    const std::string_view target,
    const std::string_view replacement) {
    std::string result{source};
    const std::size_t position = result.find(target);
    if (position != std::string::npos) {
        result.replace(position, target.size(), replacement);
    }
    return result;
}

void TestCsvSyntax(TestContext& context) {
    const data::CsvParseResult parsed = data::Csv::Parse(
        "\xEF\xBB\xBFname,value\r\n\"Room, \"\"Alpha\"\"\",42\r\n");
    context.Expect(parsed.Succeeded(), "CSV accepts a BOM, CRLF, and quoted fields");
    if (parsed.document.has_value()) {
        context.Expect(
            parsed.document->records.size() == 1 &&
                parsed.document->records.front().fields.front() == "Room, \"Alpha\"",
            "CSV decodes quoted content without leaking file acquisition policy");
    }
    context.Expect(
        !data::Csv::Parse("a,b\n\"unterminated,2").Succeeded(),
        "CSV rejects unterminated quoted fields");
}

void TestCatalogValuesAndAssetIds(TestContext& context) {
    const std::string unorderedLevels =
        "level_id,level_name,map_asset_id,next_level_id,ranged_enemy_count,melee_enemy_count,active_enemy_limit,clear_kill_count\n"
        "room_1,ROOM 1,object_fps.map.room_1,,3,3,3,5\n"
        "room_0,\"ROOM, ZERO\",object_fps.map.room_0,room_1,2,2,2,3\n";
    const GameDataLoadResult result = ParseWith(unorderedLevels);
    context.Expect(result.Succeeded(), "valid catalogs parse without filesystem access");
    if (!result.catalog.has_value()) {
        return;
    }

    const GameDataCatalog& catalog = *result.catalog;
    const EnemyDefinition* melee = catalog.enemies.FindByKind(EnemyKind::Melee);
    context.Expect(
        melee != nullptr && melee->textureAssetId == Engine::Asset::AssetId::FromString(
            "object_fps.texture.enemy.blood_dog") &&
            melee->animations.attacking.eventFrameIndex == 3U,
        "enemy definitions retain opaque texture IDs and animation policy");
    const WeaponDefinition* weapon = catalog.weapons.GetDefaultWeapon();
    context.Expect(
        weapon != nullptr &&
            weapon->presentationAssetId == Engine::Asset::AssetId::FromString(
                "object_fps.weapon.mark23"),
        "weapon definitions retain opaque presentation IDs");
    const auto levels = catalog.levels.GetDefinitions();
    context.Expect(
        levels.size() == 2 && levels[0].id == "room_0" && levels[1].id == "room_1",
        "catalog order follows the next_level_id chain, not CSV row order");
    context.Expect(
        levels[0].name == "ROOM, ZERO" &&
            levels[1].mapAssetId == Engine::Asset::AssetId::FromString(
                "object_fps.map.room_1") &&
            !levels[1].nextLevelId.has_value(),
        "level definitions retain display text, asset identity, and terminal state");
}

void TestVariableCampaignCardinality(TestContext& context) {
    for (const std::size_t count : {std::size_t{1}, std::size_t{2}, std::size_t{4}}) {
        const GameDataLoadResult result = ParseWith(MakeLevels(count));
        context.Expect(result.Succeeded(), "campaign accepts a non-hardcoded level count");
        if (!result.catalog.has_value()) {
            continue;
        }
        const auto levels = result.catalog->levels.GetDefinitions();
        context.Expect(levels.size() == count, "campaign cardinality comes from level data");
        for (std::size_t index = 0; index < levels.size(); ++index) {
            const bool hasExpectedNext = index + 1 < levels.size();
            context.Expect(
                levels[index].nextLevelId.has_value() == hasExpectedNext,
                "only the data-defined terminal level has no successor");
        }
    }
}

void TestAssetIdValidation(TestContext& context) {
    const std::string pathLike = ReplaceOnce(
        kValidWeapons,
        "object_fps.weapon.mark23",
        "../gun.png");
    ExpectRejected(
        context,
        ParseWith(kValidEnemies, kValidEnemyAnimations, pathLike, MakeLevels(1)),
        "asset ID",
        "relative paths cannot masquerade as runtime asset identity");

    const std::string uppercase = ReplaceOnce(
        kValidEnemies,
        "object_fps.texture.enemy.blood_dog",
        "Object_FPS.Texture.Enemy");
    ExpectRejected(
        context,
        ParseWith(uppercase, kValidEnemyAnimations, kValidWeapons, MakeLevels(1)),
        "asset ID",
        "asset IDs use stable lowercase syntax");
}

void TestCatalogGraphValidation(TestContext& context) {
    const std::string unknown =
        "level_id,level_name,map_asset_id,next_level_id,ranged_enemy_count,melee_enemy_count,active_enemy_limit,clear_kill_count\n"
        "room_0,ROOM 0,object_fps.map.room_0,missing,2,2,2,3\n";
    ExpectRejected(
        context, ParseWith(unknown), "does not exist", "unknown next level is rejected");

    const std::string cycle =
        "level_id,level_name,map_asset_id,next_level_id,ranged_enemy_count,melee_enemy_count,active_enemy_limit,clear_kill_count\n"
        "room_0,ROOM 0,object_fps.map.room_0,room_1,2,2,2,3\n"
        "room_1,ROOM 1,object_fps.map.room_1,room_0,2,2,2,3\n";
    ExpectRejected(context, ParseWith(cycle), "start level", "cyclic campaign is rejected");

    const std::string disconnected =
        "level_id,level_name,map_asset_id,next_level_id,ranged_enemy_count,melee_enemy_count,active_enemy_limit,clear_kill_count\n"
        "room_0,ROOM 0,object_fps.map.room_0,,2,2,2,3\n"
        "room_1,ROOM 1,object_fps.map.room_1,,2,2,2,3\n";
    ExpectRejected(
        context,
        ParseWith(disconnected),
        "exactly one start level",
        "disconnected campaign is rejected");
}

void TestScalarAndAnimationValidation(TestContext& context) {
    const std::string zeroHealth = ReplaceOnce(kValidEnemies, ",50,5,", ",0,5,");
    ExpectRejected(
        context,
        ParseWith(zeroHealth, kValidEnemyAnimations, kValidWeapons, MakeLevels(1)),
        "greater than zero",
        "non-positive enemy health is rejected");

    const std::string missingAnimation = ReplaceOnce(
        kValidEnemyAnimations,
        "melee_basic,dead,0,1380,4,0.1,,,\n",
        "");
    ExpectRejected(
        context,
        ParseWith(kValidEnemies, missingAnimation, kValidWeapons, MakeLevels(1)),
        "missing state 'dead'",
        "each enemy requires every runtime animation state");
}

void TestWeaponActionTimingData(TestContext& context) {
    const auto parsed = ParseWith(MakeLevels(1));
    if (!parsed.catalog) return;
    const auto* weapon = parsed.catalog->weapons.GetDefaultWeapon();
    context.Expect(weapon && NearlyEqual(weapon->fireIntervalSeconds, 10.0f / 30.0f) &&
                       NearlyEqual(weapon->reloadSeconds, 112.0f / 30.0f) &&
                       NearlyEqual(weapon->drawSeconds, 25.0f / 30.0f) &&
                       NearlyEqual(weapon->hideSeconds, 11.0f / 30.0f),
                   "weapon catalog retains native mark23 action durations");
    const auto zeroDraw = ReplaceOnce(kValidWeapons, ",0.8333333333,", ",0,");
    ExpectRejected(context, ParseWith(kValidEnemies, kValidEnemyAnimations, zeroDraw, MakeLevels(1)),
                   "draw_seconds", "zero draw duration is rejected by catalog validation");
    const auto invalidHide = ReplaceOnce(kValidWeapons, ",0.3666666667,", ",nan,");
    ExpectRejected(context, ParseWith(kValidEnemies, kValidEnemyAnimations, invalidHide, MakeLevels(1)),
                   "hide_seconds", "non-finite hide duration is rejected by catalog validation");
}

} // namespace

void RunGameDataCatalogTests(TestContext& context) {
    TestWeaponActionTimingData(context);
    TestCsvSyntax(context);
    TestCatalogValuesAndAssetIds(context);
    TestVariableCampaignCardinality(context);
    TestAssetIdValidation(context);
    TestCatalogGraphValidation(context);
    TestScalarAndAnimationValidation(context);
}

} // namespace fps::tests
