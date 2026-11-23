#pragma once

// Unicode's Default_Ignorable_Code_Point property (generated table,
// DefaultIgnorable.cpp). Fallback skips these characters when it checks a
// font's coverage (Design Invariant TX-5).
namespace GYO::Engine::Text::Detail {

[[nodiscard]] bool IsDefaultIgnorable(char32_t cp) noexcept;

} // namespace GYO::Engine::Text::Detail
