#pragma once

// What an author puts in a World to show a gyo.ui document, and what world_ui derives from it
// (docs/architecture/target/v2/ui.zh-Hant.md, section 4.6). world_ui registers these in the World the UI lives in;
// ui_render reads UiRoot and UiRootState through Extract (as render's drawables are registered by world_render and
// read by sprite_render). One document is one entity (section 3.1: the document is a tree of its own, not entities);
// that entity plays the part of Bevy's UI root node, its camera the part of UiTargetCamera.
//
// None of these is decided state: the UI is what a player sees, different on every machine (the window's size, the
// fonts loaded), so the hashes write nothing (as input's and camera's derived values).

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <variant>

#include "engine/asset/Handle.hpp"
#include "engine/image/ImageData.hpp"
#include "engine/kernel/Hash.hpp"
#include "engine/kernel/World.hpp"
#include "engine/ui/UiAsset.hpp"
#include "engine/ui/UiRuntime.hpp"
#include "engine/ui/UiText.hpp"

namespace GYO::Engine::Ui {

// The document a root shows: none, a gyo.ui asset (shown once it and its fonts are loaded), or a document the author
// holds (a tool's, a test's; its fonts come through UiText::AddFont).
using UiDocumentRef = std::variant<std::monostate, Asset::Handle<UiAsset>, std::shared_ptr<const UiDocument>>;

// One document on screen (the author's).
struct UiRoot final {
    UiDocumentRef document;
    // The canvas shown; changing it activates the new one (the focus goes to its default).
    std::string canvas;
    // The camera whose viewport it is laid out in and drawn over; none: the camera on target 0 with the largest
    // (order, entity), as Bevy's DefaultUiCamera.
    std::optional<Kernel::EntityId> camera;
    // Roots on one camera are drawn by (order, entity), the larger later (on top).
    std::int32_t order = 0;
    // False: the frame's input is not given to this root (a HUD under a menu).
    bool acceptsInput = true;
    // The text fields' character rule (UiRuntime::SetTextFilter); empty: every committed character.
    UiTextFilter filter;
};

// The binding values a root's document reads (the author's). A root without one has none. A vector, not a map: a
// component moves without throwing (the kernel's W-8), and MSVC's node containers allocate when they are moved.
struct UiBindings final {
    // One entry per binding id.
    std::vector<std::pair<std::string, UiBindingValue>> values;

    // Sets the value of id, adding it when it is not there.
    void Set(std::string id, UiBindingValue value) {
        for (auto& entry : values) {
            if (entry.first == id) {
                entry.second = std::move(value);
                return;
            }
        }
        values.emplace_back(std::move(id), std::move(value));
    }
    [[nodiscard]] const UiBindingValue* Find(std::string_view id) const {
        for (const auto& entry : values) {
            if (entry.first == id) return &entry.second;
        }
        return nullptr;
    }
    // The values as UiRuntime takes them.
    [[nodiscard]] UiBindingTable Table() const { return UiBindingTable(values.begin(), values.end()); }
};

// What world_ui derives for a root each frame (written only by world_ui).
struct UiRootState final {
    // The runtime of the document shown; none until the document is loaded. Held by value, so a reader through
    // Extract<Read<UiRootState>> gets it const (UI-7).
    std::optional<UiRuntime> runtime;
    // The camera the root was laid out in this frame; none: no valid camera (nothing is drawn).
    std::optional<Kernel::EntityId> camera;
    // That camera's viewport: its top-left corner on the target and its size, in physical pixels, and the physical
    // pixels per logical pixel.
    Math::Vec2 viewportPosition{};
    UiViewport viewport{};
    float scaleFactor = 1.0F;
    // This frame's draw list, in pixels of the viewport; empty when nothing is drawn.
    UiDrawList drawList;
    // The text field being edited, in pixels of the viewport.
    std::optional<UiEditArea> editArea;
    // The pictures of the document's image elements, requested by their asset id (a vector for the reason UiBindings
    // has one).
    std::vector<std::pair<std::string, Asset::Handle<Image::ImageData>>> textures;

    [[nodiscard]] const Asset::Handle<Image::ImageData>* Texture(std::string_view id) const {
        for (const auto& entry : textures) {
            if (entry.first == id) return &entry.second;
        }
        return nullptr;
    }
    // What went wrong this frame (a missing binding, a document that did not validate); the root is not drawn while
    // it is set (E2: this root, not the World).
    std::optional<UiFailure> failure;
};

// An action of a root's document (a button pressed, a slider moved, a text field submitted, the canvas cancelled),
// as a Message.
struct UiAction final {
    Kernel::EntityId root{};
    std::string action;
    std::string element;
    UiActionPayload payload;
};

inline void HashValue(Kernel::Hasher&, const UiRoot&) {}
inline void HashValue(Kernel::Hasher&, const UiBindings&) {}
inline void HashValue(Kernel::Hasher&, const UiRootState&) {}
inline void HashValue(Kernel::Hasher&, const UiAction&) {}
inline void HashValue(Kernel::Hasher&, const UiInput&) {}
inline void HashValue(Kernel::Hasher&, const UiText&) {}

} // namespace GYO::Engine::Ui
