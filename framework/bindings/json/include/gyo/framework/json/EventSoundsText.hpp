#pragma once

#include "gyo/framework/EventSounds.hpp"
#include "gyo/framework/FrameworkError.hpp"

#include <string_view>

// The JSON binding (framework/bindings/json, target GYO::ToolFrameworkJson):
// gyo.event_sounds version 1 text (docs/framework/event-sounds.md). The text
// rules are here; every rule on the values is the core's CheckEventSoundSet.

namespace Gyo::Tools::Framework {

// A gyo.event_sounds text as structure: one JSON object with exactly its
// fields, no repeated key in any object, schema and version, an object per
// event with exactly sound_asset_id, sync_seconds and gain, then
// CheckEventSoundSet. InvalidEventSounds with the path of the offending value
// otherwise. Binding is the core's BindEventSounds.
[[nodiscard]] Result<EventSoundSet, EventSoundsError> ParseEventSounds(std::string_view text);

} // namespace Gyo::Tools::Framework
