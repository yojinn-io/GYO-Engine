#include "engine/logging/JsonLines.hpp"

#include <cmath>
#include <cstdio>
#include <string_view>

#include "engine/json/Writer.hpp"

namespace GYO::Engine::Logging {

namespace {

namespace Json = GYO::Engine::Json;

// Days since 1970-01-01 to a proleptic Gregorian date (Howard Hinnant's civil_from_days).
void CivilFromDays(std::int64_t days, int& year, int& month, int& day) noexcept {
    days += 719468;
    const std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    const auto dayOfEra = static_cast<std::uint32_t>(days - era * 146097);
    const std::uint32_t yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
    const std::uint32_t dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
    const std::uint32_t monthIndex = (5 * dayOfYear + 2) / 153;
    day = static_cast<int>(dayOfYear - (153 * monthIndex + 2) / 5 + 1);
    month = static_cast<int>(monthIndex < 10 ? monthIndex + 3 : monthIndex - 9);
    year = static_cast<int>(static_cast<std::int64_t>(yearOfEra) + era * 400 + (month <= 2 ? 1 : 0));
}

// "2026-11-20T10:40:11.007+0900": the local date and time, then the offset as %z writes it.
std::string_view FormatWall(const WallTime wall, char (&buffer)[48]) noexcept {
    constexpr std::int64_t kMsPerDay = 86'400'000;
    const std::int64_t localMs = wall.unixMs + std::int64_t{wall.utcOffsetSeconds} * 1000;
    std::int64_t days = localMs / kMsPerDay;
    std::int64_t msOfDay = localMs % kMsPerDay;
    if (msOfDay < 0) {
        msOfDay += kMsPerDay;
        --days;
    }
    int year = 0;
    int month = 0;
    int day = 0;
    CivilFromDays(days, year, month, day);
    const int hour = static_cast<int>(msOfDay / 3'600'000);
    const int minute = static_cast<int>(msOfDay / 60'000 % 60);
    const int second = static_cast<int>(msOfDay / 1000 % 60);
    const int millisecond = static_cast<int>(msOfDay % 1000);
    const int offsetMinutes = wall.utcOffsetSeconds / 60;
    const char sign = offsetMinutes < 0 ? '-' : '+';
    const int absoluteMinutes = offsetMinutes < 0 ? -offsetMinutes : offsetMinutes;
    const int written = std::snprintf(buffer, sizeof buffer, "%04d-%02d-%02dT%02d:%02d:%02d.%03d%c%02d%02d", year, month,
                                      day, hour, minute, second, millisecond, sign, absoluteMinutes / 60,
                                      absoluteMinutes % 60);
    if (written <= 0) return {};
    return std::string_view(buffer, static_cast<std::size_t>(written) < sizeof buffer ? static_cast<std::size_t>(written)
                                                                                    : sizeof buffer - 1);
}

void WriteDouble(Json::Writer& writer, const double value) {
    if (std::isnan(value)) {
        writer.String("NaN");
    } else if (std::isinf(value)) {
        writer.String(value > 0 ? "Infinity" : "-Infinity");
    } else {
        writer.Double(value);
    }
}

Json::JsonResult<void> WriteRecord(std::string& out, const Record& record) {
    Json::Writer writer(out);
    writer.BeginObject();
    writer.Key("schema");
    writer.Uint(kJsonLinesSchema);
    writer.Key("kind");
    writer.String(ToString(record.kind));
    writer.Key("level");
    writer.String(ToString(record.level));
    writer.Key("category");
    writer.String(record.category);
    char wall[48];
    writer.Key("wall");
    writer.String(FormatWall(record.wall, wall));
    writer.Key("mono_ns");
    writer.Int(record.monoNs);
    if (record.kind == RecordKind::Span) {
        writer.Key("begin_ns");
        writer.Int(record.beginNs);
        writer.Key("duration_ns");
        writer.Int(record.durationNs);
    }
    if (!record.product.empty()) {
        writer.Key("product");
        writer.String(record.product);
    }
    if (!record.world.empty()) {
        writer.Key("world");
        writer.String(record.world);
    }
    if (record.worldInterrupted) {
        writer.Key("world_interrupted");
        writer.Bool(true);
    }
    writer.Key("text");
    writer.String(record.text);
    writer.Key("fields");
    writer.BeginObject();
    for (const Field& field : record.fields) {
        writer.Key(field.key);
        switch (field.value.GetType()) {
        case FieldValue::Type::Int: writer.Int(field.value.AsInt()); break;
        case FieldValue::Type::Uint: writer.Uint(field.value.AsUint()); break;
        case FieldValue::Type::Double: WriteDouble(writer, field.value.AsDouble()); break;
        case FieldValue::Type::Bool: writer.Bool(field.value.AsBool()); break;
        case FieldValue::Type::String: writer.String(field.value.AsString()); break;
        }
    }
    writer.EndObject();
    writer.EndObject();
    return writer.Finish();
}

} // namespace

bool AppendJsonLine(std::string& out, const Record& record) {
    auto written = WriteRecord(out, record);
    if (written) {
        out += '\n';
        return true;
    }
    // The Writer cut out back to where it was. The Error record that replaces
    // this one names the JSON error, not the field (JsonError has none).
    std::string text = "a record could not be written as JSON: ";
    text += ToString(written.error().code);
    text += ": ";
    text += written.error().message;
    Record replacement{.kind = record.kind,
                       .level = Level::Error,
                       .category = "logging",
                       .text = text,
                       .wall = record.wall,
                       .monoNs = record.monoNs,
                       .product = record.product,
                       .world = record.world,
                       .worldInterrupted = record.worldInterrupted,
                       .beginNs = record.beginNs,
                       .durationNs = record.durationNs};
    if (!WriteRecord(out, replacement)) {
        // The product or the World name itself is what JSON cannot represent.
        replacement.product = {};
        replacement.world = {};
        static_cast<void>(WriteRecord(out, replacement));
    }
    out += '\n';
    return false;
}

void AppendJsonEndLine(std::string& out, const std::int64_t monoNs, const JsonLinesCounts& counts) {
    Json::Writer writer(out);
    writer.BeginObject();
    writer.Key("schema");
    writer.Uint(kJsonLinesSchema);
    writer.Key("kind");
    writer.String("end");
    writer.Key("mono_ns");
    writer.Int(monoNs);
    writer.Key("written");
    writer.Uint(counts.written);
    writer.Key("dropped_full");
    writer.Uint(counts.droppedFull);
    writer.Key("dropped_closed");
    writer.Uint(counts.droppedClosed);
    writer.Key("fatal_direct");
    writer.Uint(counts.fatalDirect);
    writer.EndObject();
    if (writer.Finish()) out += '\n';
}

} // namespace GYO::Engine::Logging
