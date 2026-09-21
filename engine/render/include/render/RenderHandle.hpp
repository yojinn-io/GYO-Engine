#pragma once

#include <cstdint>
#include <functional>

namespace Engine::Render {

template <class Tag>
class ResourceHandle final {
public:
    constexpr ResourceHandle() noexcept = default;

    [[nodiscard]] static constexpr ResourceHandle FromParts(
        std::uint32_t index,
        std::uint32_t generation) noexcept {
        return ResourceHandle(index, generation);
    }

    [[nodiscard]] constexpr std::uint32_t Index() const noexcept {
        return index_;
    }

    [[nodiscard]] constexpr std::uint32_t Generation() const noexcept {
        return generation_;
    }

    [[nodiscard]] constexpr bool IsValid() const noexcept {
        return generation_ != 0;
    }

    explicit constexpr operator bool() const noexcept {
        return IsValid();
    }

    friend constexpr bool operator==(
        const ResourceHandle&,
        const ResourceHandle&) noexcept = default;

private:
    constexpr ResourceHandle(
        std::uint32_t index,
        std::uint32_t generation) noexcept
        : index_(index), generation_(generation) {}

    std::uint32_t index_{};
    std::uint32_t generation_{};
};

struct MeshHandleTag;
struct TextureHandleTag;
struct PipelineHandleTag;
struct ShaderHandleTag;

using MeshHandle = ResourceHandle<MeshHandleTag>;
using TextureHandle = ResourceHandle<TextureHandleTag>;
using PipelineHandle = ResourceHandle<PipelineHandleTag>;
using ShaderHandle = ResourceHandle<ShaderHandleTag>;

} // namespace Engine::Render

namespace std {

template <class Tag>
struct hash<Engine::Render::ResourceHandle<Tag>> final {
    std::size_t operator()(
        const Engine::Render::ResourceHandle<Tag>& handle) const noexcept {
        const std::uint64_t value =
            (static_cast<std::uint64_t>(handle.Generation()) << 32U) |
            handle.Index();
        return std::hash<std::uint64_t>{}(value);
    }
};

} // namespace std
