#include "RetroFPS/Data/GameData.hpp"

#include "RetroFPS/Data/Csv.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fps {
namespace {

constexpr std::string_view kEnemyCatalogName{"enemies.csv"};
constexpr std::string_view kWeaponCatalogName{"weapons.csv"};
constexpr std::string_view kLevelCatalogName{"levels.csv"};

const std::vector<std::string> kEnemyHeader{
    "enemy_id",
    "kind",
    "damage",
    "attack_interval_seconds",
    "hp",
    "defense",
    "hitbox_radius",
    "hitbox_height",
    "presentation_asset_id",
};
const std::vector<std::string> kWeaponHeader{
    "weapon_id",
    "damage",
    "magazine_size",
    "reserve_ammo",
    "recoil",
    "automatic",
    "fire_interval_seconds",
    "reload_seconds",
    "draw_seconds",
    "hide_seconds",
    "presentation_asset_id",
};
const std::vector<std::string> kLevelHeader{
    "level_id",
    "level_name",
    "map_asset_id",
    "next_level_id",
    "ranged_enemy_count",
    "melee_enemy_count",
    "active_enemy_limit",
    "clear_kill_count",
};

class GameDataError final : public std::runtime_error {
public:
    explicit GameDataError(const std::string& message)
        : std::runtime_error(message) {}
};

struct ParsedCatalogData final {
    std::vector<EnemyDefinition> enemies;
    std::vector<WeaponDefinition> weapons;
    std::vector<LevelDefinition> levels;
};

[[nodiscard]] std::string JoinHeader(const std::vector<std::string>& header) {
    std::string result;
    for (std::size_t index = 0; index < header.size(); ++index) {
        if (index != 0) {
            result.push_back(',');
        }
        result += header[index];
    }
    return result;
}

void ValidateHeader(
    const data::CsvDocument& document,
    const std::vector<std::string>& expected,
    const std::string_view catalogName) {
    if (document.header == expected) {
        return;
    }
    throw GameDataError(
        std::string{catalogName} + " header must be exactly: " + JoinHeader(expected));
}

[[noreturn]] void ThrowFieldError(
    const std::string_view catalogName,
    const data::CsvRecord& record,
    const std::string_view fieldName,
    const std::string& detail) {
    const std::vector<std::string>* header = nullptr;
    if (catalogName == kEnemyCatalogName) {
        header = &kEnemyHeader;

    } else if (catalogName == kWeaponCatalogName) {
        header = &kWeaponHeader;
    } else if (catalogName == kLevelCatalogName) {
        header = &kLevelHeader;
    }
    std::size_t column = 0;
    if (header != nullptr) {
        const auto found = std::ranges::find(*header, fieldName);
        if (found != header->end()) {
            column = static_cast<std::size_t>(std::distance(header->begin(), found)) + 1;
        }
    }
    throw GameDataError(
        std::string{catalogName} + " line " + std::to_string(record.lineNumber) +
        ", column " + std::to_string(column) + ", field '" +
        std::string{fieldName} + "': " + detail);
}

[[nodiscard]] std::uint32_t ParseUnsigned(
    const std::string& text,
    const std::string_view catalogName,
    const data::CsvRecord& record,
    const std::string_view fieldName) {
    std::uint32_t value = 0;
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const std::from_chars_result result = std::from_chars(begin, end, value, 10);
    if (text.empty() || result.ec != std::errc{} || result.ptr != end) {
        ThrowFieldError(catalogName, record, fieldName, "expected an unsigned 32-bit integer");
    }
    return value;
}

[[nodiscard]] std::string ParseDefinitionId(
    const std::string& text,
    const std::string_view catalogName,
    const data::CsvRecord& record,
    const std::string_view fieldName) {
    if (text.empty() || text.front() < 'a' || text.front() > 'z' ||
        text.back() == '_') {
        ThrowFieldError(
            catalogName,
            record,
            fieldName,
            "ID must use lower_snake_case and begin with a lowercase letter");
    }
    bool previousUnderscore = false;
    for (const char character : text) {
        const bool lowercaseLetter = character >= 'a' && character <= 'z';
        const bool digit = character >= '0' && character <= '9';
        const bool underscore = character == '_';
        if ((!lowercaseLetter && !digit && !underscore) ||
            (underscore && previousUnderscore)) {
            ThrowFieldError(
                catalogName,
                record,
                fieldName,
                "ID must use lower_snake_case and contain no repeated underscores");
        }
        previousUnderscore = underscore;
    }
    return text;
}

[[nodiscard]] float ParseFloat(
    const std::string& text,
    const std::string_view catalogName,
    const data::CsvRecord& record,
    const std::string_view fieldName) {
    // Keep from_chars' decimal grammar: no whitespace, leading '+', hex, or
    // non-finite spellings. Do not depend on the process-wide numeric locale.
    std::size_t index = 0;
    if (!text.empty() && text.front() == '-') {
        ++index;
    }
    bool hasSignificandDigit = false;
    bool hasNonzeroSignificand = false;
    const auto consumeDigits = [&](const bool significand) {
        const std::size_t begin = index;
        while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
            if (significand) {
                hasSignificandDigit = true;
                hasNonzeroSignificand |= text[index] != '0';
            }
            ++index;
        }
        return index != begin;
    };
    consumeDigits(true);
    if (index < text.size() && text[index] == '.') {
        ++index;
        consumeDigits(true);
    }
    bool validExponent = true;
    if (index < text.size() && (text[index] == 'e' || text[index] == 'E')) {
        ++index;
        if (index < text.size() && (text[index] == '+' || text[index] == '-')) {
            ++index;
        }
        validExponent = consumeDigits(false);
    }
    if (!hasSignificandDigit || !validExponent || index != text.size()) {
        ThrowFieldError(catalogName, record, fieldName, "expected a finite decimal number");
    }

    // Xcode 16's libc++ has no floating-point from_chars overload. Classic-locale
    // num_get is available on every supported toolchain and converts directly
    // to float, avoiding a second rounding through double.
    float value = 0.0f;
    std::istringstream stream{text};
    stream.imbue(std::locale::classic());
    stream >> std::noskipws >> value;
    // libc++ may flag underflow even when the result is a representable
    // subnormal (or rounds up to the smallest normal). Keep those results, but
    // reject overflow and nonzero input that underflows all the way to zero.
    const bool representableUnderflow =
        value != 0.0f && std::abs(value) <= std::numeric_limits<float>::min();
    if (!stream.eof() || stream.bad() || (stream.fail() && !representableUnderflow) ||
        !std::isfinite(value) || (value == 0.0f && hasNonzeroSignificand)) {
        ThrowFieldError(catalogName, record, fieldName, "expected a finite decimal number");
    }
    return value;
}

[[nodiscard]] bool ParseBoolean(
    const std::string& text,
    const data::CsvRecord& record,
    const std::string_view fieldName) {
    if (text == "true") {
        return true;
    }
    if (text == "false") {
        return false;
    }
    ThrowFieldError(kWeaponCatalogName, record, fieldName, "expected exactly 'true' or 'false'");
}

[[nodiscard]] bool HasVisibleText(const std::string& text) noexcept {
    return std::any_of(text.begin(), text.end(), [](const unsigned char character) {
        return character != ' ' && character != '\t' && character != '\r' && character != '\n';
    });
}

[[nodiscard]] Engine::Asset::AssetId ParseRuntimeAssetId(
    const std::string& text,
    const std::string_view catalogName,
    const data::CsvRecord& record,
    const std::string_view fieldName) {
    if (text.empty() || text.find('\0') != std::string::npos ||
        text.find('\r') != std::string::npos || text.find('\n') != std::string::npos) {
        ThrowFieldError(catalogName, record, fieldName, "asset ID must be non-empty and single-line");
    }
    const auto validCharacter = [](const unsigned char character) noexcept {
        return (character >= 'a' && character <= 'z') ||
               (character >= '0' && character <= '9') || character == '.' ||
               character == '_' || character == '-';
    };
    if (!std::all_of(text.begin(), text.end(), validCharacter) ||
        text.front() == '.' || text.front() == '-' || text.back() == '.' ||
        text.back() == '-' || text.find("..") != std::string::npos) {
        ThrowFieldError(
            catalogName,
            record,
            fieldName,
            "asset ID must use lowercase letters, digits, '.', '_', or '-' with stable segments");
    }
    return Engine::Asset::AssetId::FromString(text);
}

void ValidatePositive(
    const float value,
    const std::string_view catalogName,
    const data::CsvRecord& record,
    const std::string_view fieldName) {
    if (value <= 0.0f) {
        ThrowFieldError(catalogName, record, fieldName, "value must be greater than zero");
    }
}

void ValidateNonNegative(
    const float value,
    const std::string_view catalogName,
    const data::CsvRecord& record,
    const std::string_view fieldName) {
    if (value < 0.0f) {
        ThrowFieldError(catalogName, record, fieldName, "value must be non-negative");
    }
}

[[nodiscard]] std::vector<EnemyDefinition> ParseEnemies(
    const data::CsvDocument& document) {
    ValidateHeader(document, kEnemyHeader, kEnemyCatalogName);
    if (document.records.empty()) {
        throw GameDataError("enemy CSV must contain at least one data record");
    }

    std::vector<EnemyDefinition> definitions;
    definitions.reserve(document.records.size());
    std::unordered_set<EnemyDefinitionId> ids;
    bool hasMelee = false;
    bool hasRanged = false;

    for (const data::CsvRecord& record : document.records) {
        EnemyDefinition definition;
        definition.id = ParseDefinitionId(
            record.fields[0], kEnemyCatalogName, record, kEnemyHeader[0]);
        if (!ids.insert(definition.id).second) {
            ThrowFieldError(kEnemyCatalogName, record, kEnemyHeader[0], "duplicate enemy ID");
        }

        if (record.fields[1] == "melee") {
            if (hasMelee) {
                ThrowFieldError(
                    kEnemyCatalogName, record, kEnemyHeader[1], "only one melee definition is allowed");
            }
            hasMelee = true;
            definition.kind = EnemyKind::Melee;
        } else if (record.fields[1] == "ranged") {
            if (hasRanged) {
                ThrowFieldError(
                    kEnemyCatalogName, record, kEnemyHeader[1], "only one ranged definition is allowed");
            }
            hasRanged = true;
            definition.kind = EnemyKind::Ranged;
        } else {
            ThrowFieldError(
                kEnemyCatalogName, record, kEnemyHeader[1], "expected exactly 'melee' or 'ranged'");
        }

        definition.damage = ParseFloat(
            record.fields[2], kEnemyCatalogName, record, kEnemyHeader[2]);
        definition.attackIntervalSeconds = ParseFloat(
            record.fields[3], kEnemyCatalogName, record, kEnemyHeader[3]);
        definition.maxHealth = ParseFloat(
            record.fields[4], kEnemyCatalogName, record, kEnemyHeader[4]);
        definition.defense = ParseFloat(
            record.fields[5], kEnemyCatalogName, record, kEnemyHeader[5]);
        definition.hitboxRadius = ParseFloat(
            record.fields[6], kEnemyCatalogName, record, kEnemyHeader[6]);
        definition.hitboxHeight = ParseFloat(
            record.fields[7], kEnemyCatalogName, record, kEnemyHeader[7]);
        ValidatePositive(definition.damage, kEnemyCatalogName, record, kEnemyHeader[2]);
        ValidatePositive(
            definition.attackIntervalSeconds, kEnemyCatalogName, record, kEnemyHeader[3]);
        ValidatePositive(definition.maxHealth, kEnemyCatalogName, record, kEnemyHeader[4]);
        ValidateNonNegative(definition.defense, kEnemyCatalogName, record, kEnemyHeader[5]);
        ValidatePositive(definition.hitboxRadius, kEnemyCatalogName, record, kEnemyHeader[6]);
        ValidatePositive(definition.hitboxHeight, kEnemyCatalogName, record, kEnemyHeader[7]);
        if (definition.hitboxHeight < 2*definition.hitboxRadius)
            ThrowFieldError(kEnemyCatalogName, record, kEnemyHeader[7], "capsule height must include both hemispheres");
        definition.presentationAssetId = ParseRuntimeAssetId(record.fields[8], kEnemyCatalogName, record, kEnemyHeader[8]);
        definitions.push_back(std::move(definition));
    }

    if (!hasMelee || !hasRanged) {
        throw GameDataError("enemy CSV must contain exactly one melee and one ranged definition");
    }
    return definitions;
}

[[nodiscard]] std::vector<WeaponDefinition> ParseWeapons(
    const data::CsvDocument& document) {
    ValidateHeader(document, kWeaponHeader, kWeaponCatalogName);
    if (document.records.empty()) {
        throw GameDataError("weapon CSV must contain at least one data record");
    }

    std::vector<WeaponDefinition> definitions;
    definitions.reserve(document.records.size());
    std::unordered_set<WeaponDefinitionId> ids;
    for (const data::CsvRecord& record : document.records) {
        WeaponDefinition definition;
        definition.id = ParseDefinitionId(
            record.fields[0], kWeaponCatalogName, record, kWeaponHeader[0]);
        if (!ids.insert(definition.id).second) {
            ThrowFieldError(kWeaponCatalogName, record, kWeaponHeader[0], "duplicate weapon ID");
        }

        definition.damage = ParseFloat(
            record.fields[1], kWeaponCatalogName, record, kWeaponHeader[1]);
        definition.magazineCapacity = ParseUnsigned(
            record.fields[2], kWeaponCatalogName, record, kWeaponHeader[2]);
        definition.reserveAmmo = ParseUnsigned(
            record.fields[3], kWeaponCatalogName, record, kWeaponHeader[3]);
        definition.recoilDegrees = ParseFloat(
            record.fields[4], kWeaponCatalogName, record, kWeaponHeader[4]);
        definition.automatic = ParseBoolean(record.fields[5], record, kWeaponHeader[5]);
        definition.fireIntervalSeconds = ParseFloat(
            record.fields[6], kWeaponCatalogName, record, kWeaponHeader[6]);
        definition.reloadSeconds = ParseFloat(
            record.fields[7], kWeaponCatalogName, record, kWeaponHeader[7]);
        definition.drawSeconds = ParseFloat(
            record.fields[8], kWeaponCatalogName, record, kWeaponHeader[8]);
        definition.hideSeconds = ParseFloat(
            record.fields[9], kWeaponCatalogName, record, kWeaponHeader[9]);
        definition.presentationAssetId = ParseRuntimeAssetId(
            record.fields[10], kWeaponCatalogName, record, kWeaponHeader[10]);

        ValidatePositive(definition.damage, kWeaponCatalogName, record, kWeaponHeader[1]);
        if (definition.magazineCapacity == 0) {
            ThrowFieldError(
                kWeaponCatalogName, record, kWeaponHeader[2], "value must be greater than zero");
        }
        ValidateNonNegative(
            definition.recoilDegrees, kWeaponCatalogName, record, kWeaponHeader[4]);
        ValidatePositive(
            definition.fireIntervalSeconds, kWeaponCatalogName, record, kWeaponHeader[6]);
        ValidatePositive(
            definition.reloadSeconds, kWeaponCatalogName, record, kWeaponHeader[7]);
        ValidatePositive(definition.drawSeconds, kWeaponCatalogName, record, kWeaponHeader[8]);
        ValidatePositive(definition.hideSeconds, kWeaponCatalogName, record, kWeaponHeader[9]);
        definitions.push_back(std::move(definition));
    }
    return definitions;
}

[[nodiscard]] std::vector<LevelDefinition> ParseLevels(
    const data::CsvDocument& document) {
    ValidateHeader(document, kLevelHeader, kLevelCatalogName);
    if (document.records.empty()) {
        throw GameDataError("level CSV must contain at least one data record");
    }

    std::vector<LevelDefinition> definitions;
    definitions.reserve(document.records.size());
    std::unordered_set<LevelDefinitionId> ids;
    for (const data::CsvRecord& record : document.records) {
        LevelDefinition definition;
        definition.id = ParseDefinitionId(
            record.fields[0], kLevelCatalogName, record, kLevelHeader[0]);
        if (!ids.insert(definition.id).second) {
            ThrowFieldError(kLevelCatalogName, record, kLevelHeader[0], "duplicate level ID");
        }

        definition.name = record.fields[1];
        if (!HasVisibleText(definition.name)) {
            ThrowFieldError(kLevelCatalogName, record, kLevelHeader[1], "name must not be empty");
        }
        definition.mapAssetId = ParseRuntimeAssetId(
            record.fields[2], kLevelCatalogName, record, kLevelHeader[2]);
        if (!record.fields[3].empty()) {
            definition.nextLevelId = ParseDefinitionId(
                record.fields[3], kLevelCatalogName, record, kLevelHeader[3]);
        }
        definition.rangedEnemyCount = ParseUnsigned(
            record.fields[4], kLevelCatalogName, record, kLevelHeader[4]);
        definition.meleeEnemyCount = ParseUnsigned(
            record.fields[5], kLevelCatalogName, record, kLevelHeader[5]);
        definition.activeEnemyLimit = ParseUnsigned(
            record.fields[6], kLevelCatalogName, record, kLevelHeader[6]);
        definition.clearKillCount = ParseUnsigned(
            record.fields[7], kLevelCatalogName, record, kLevelHeader[7]);
        if (definition.activeEnemyLimit == 0) {
            ThrowFieldError(
                kLevelCatalogName,
                record,
                kLevelHeader[6],
                "active enemy limit must be greater than zero");
        }
        if (definition.clearKillCount == 0) {
            ThrowFieldError(
                kLevelCatalogName,
                record,
                kLevelHeader[7],
                "clear kill count must be greater than zero");
        }
        definitions.push_back(std::move(definition));
    }

    std::unordered_map<LevelDefinitionId, std::size_t> indices;
    indices.reserve(definitions.size());
    for (std::size_t index = 0; index < definitions.size(); ++index) {
        indices.emplace(definitions[index].id, index);
    }
    for (std::size_t index = 0; index < definitions.size(); ++index) {
        const LevelDefinition& definition = definitions[index];
        if (definition.nextLevelId.has_value() &&
            !indices.contains(*definition.nextLevelId)) {
            ThrowFieldError(
                kLevelCatalogName,
                document.records[index],
                kLevelHeader[3],
                "referenced level ID does not exist");
        }
    }

    std::vector<std::size_t> incomingCounts(definitions.size(), 0);
    for (std::size_t index = 0; index < definitions.size(); ++index) {
        if (!definitions[index].nextLevelId.has_value()) {
            continue;
        }
        const std::size_t targetIndex = indices.at(*definitions[index].nextLevelId);
        ++incomingCounts[targetIndex];
        if (incomingCounts[targetIndex] > 1) {
            ThrowFieldError(
                kLevelCatalogName,
                document.records[index],
                kLevelHeader[3],
                "linear progression cannot merge multiple levels into one destination");
        }
    }

    std::optional<std::size_t> startIndex;
    for (std::size_t index = 0; index < incomingCounts.size(); ++index) {
        if (incomingCounts[index] != 0) {
            continue;
        }
        if (startIndex.has_value()) {
            throw GameDataError("level CSV progression must have exactly one start level");
        }
        startIndex = index;
    }
    if (!startIndex.has_value()) {
        throw GameDataError("level CSV progression contains a cycle or has no start level");
    }

    std::vector<bool> visited(definitions.size(), false);
    std::vector<LevelDefinition> ordered;
    ordered.reserve(definitions.size());
    std::optional<std::size_t> currentIndex = startIndex;
    for (std::size_t step = 0; step < definitions.size(); ++step) {
        if (!currentIndex.has_value()) {
            throw GameDataError(
                "level CSV progression terminates before visiting every level");
        }
        if (visited[*currentIndex]) {
            throw GameDataError("level CSV progression contains a cycle");
        }
        visited[*currentIndex] = true;
        ordered.push_back(definitions[*currentIndex]);
        const std::optional<LevelDefinitionId> next = definitions[*currentIndex].nextLevelId;
        currentIndex = next.has_value()
                           ? std::optional<std::size_t>{indices.at(*next)}
                           : std::nullopt;
    }
    if (currentIndex.has_value()) {
        throw GameDataError("level CSV progression must terminate with an empty next_level_id");
    }
    return ordered;
}

[[nodiscard]] ParsedCatalogData BuildCatalogData(
    const data::CsvDocument& enemies,
    const data::CsvDocument& weapons,
    const data::CsvDocument& levels) {
    ParsedCatalogData parsed{
        ParseEnemies(enemies),
        ParseWeapons(weapons),
        ParseLevels(levels),
    };
    return parsed;
}

[[nodiscard]] const data::CsvDocument& RequireDocument(
    const data::CsvParseResult& result,
    const std::string_view label) {
    if (!result.document.has_value()) {
        throw GameDataError(std::string{label} + " CSV: " + result.error);
    }
    return *result.document;
}

} // namespace

const EnemyDefinition* EnemyCatalog::FindById(const std::string_view id) const noexcept {
    const auto found = std::find_if(
        definitions_.begin(), definitions_.end(), [id](const EnemyDefinition& definition) {
            return std::string_view{definition.id} == id;
        });
    return found == definitions_.end() ? nullptr : &*found;
}

const EnemyDefinition* EnemyCatalog::FindByKind(const EnemyKind kind) const noexcept {
    const auto found = std::find_if(
        definitions_.begin(), definitions_.end(), [kind](const EnemyDefinition& definition) {
            return definition.kind == kind;
        });
    return found == definitions_.end() ? nullptr : &*found;
}

const WeaponDefinition* WeaponCatalog::FindById(const std::string_view id) const noexcept {
    const auto found = std::find_if(
        definitions_.begin(), definitions_.end(), [id](const WeaponDefinition& definition) {
            return std::string_view{definition.id} == id;
        });
    return found == definitions_.end() ? nullptr : &*found;
}

const WeaponDefinition* WeaponCatalog::GetDefaultWeapon() const noexcept {
    return definitions_.empty() ? nullptr : &definitions_.front();
}

const LevelDefinition* LevelCatalog::FindById(const std::string_view id) const noexcept {
    const auto found = std::find_if(
        definitions_.begin(), definitions_.end(), [id](const LevelDefinition& definition) {
            return std::string_view{definition.id} == id;
        });
    return found == definitions_.end() ? nullptr : &*found;
}

const LevelDefinition* LevelCatalog::GetStartLevel() const noexcept {
    return definitions_.empty() ? nullptr : &definitions_.front();
}

GameDataLoadResult GameDataLoader::Parse(
    const std::string_view enemiesCsv,
    const std::string_view weaponsCsv,
    const std::string_view levelsCsv) {
    try {
        const data::CsvParseResult enemyResult = data::Csv::Parse(enemiesCsv);
        const data::CsvParseResult weaponResult = data::Csv::Parse(weaponsCsv);
        const data::CsvParseResult levelResult = data::Csv::Parse(levelsCsv);
        ParsedCatalogData parsed = BuildCatalogData(
            RequireDocument(enemyResult, "enemy"),
            RequireDocument(weaponResult, "weapon"),
            RequireDocument(levelResult, "level"));

        GameDataCatalog catalog;
        catalog.enemies.definitions_ = std::move(parsed.enemies);
        catalog.weapons.definitions_ = std::move(parsed.weapons);
        catalog.levels.definitions_ = std::move(parsed.levels);
        return {std::move(catalog), {}};
    } catch (const std::exception& exception) {
        return {std::nullopt, exception.what()};
    }
}

} // namespace fps
