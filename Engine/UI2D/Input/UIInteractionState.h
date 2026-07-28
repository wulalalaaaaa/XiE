#pragma once

#include "UI2D/Input/UIHoverTracker.h"
#include "UI2D/Input/UIPointerCaptureService.h"
#include "UI2D/Input/UIPressTracker.h"
#include "UI2D/Input/IUIFocusManager.h"

#include <cstdint>

namespace Engine::UI2D {

struct UIInteractionRuntimeState {
    UIHoverTracker hover;
    UIPointerCaptureService capture;
    UIPressTracker press;
    std::uint64_t revision = 1;

    void Touch() noexcept { ++revision; }

    void OnNodeInvalidated(UINodeHandle node) {
        hover.OnNodeInvalidated(node);
        capture.OnNodeInvalidated(node);
        press.OnNodeInvalidated(node);
        Touch();
    }
    void ClearAll() {
        (void)hover.ClearAll();
        capture.ClearAll();
        press.ClearAll();
        Touch();
    }
    [[nodiscard]] bool HasActiveInteraction() const {
        return capture.HasCapture() || press.HasActivePress();
    }
};

struct UIInteractionSnapshot {
    const UIInteractionRuntimeState* state = nullptr;
    const IUIFocusManager* focus = nullptr;
    std::uint64_t focusRevision = 0;
    [[nodiscard]] bool IsHovered(UINodeHandle node) const {
        return state && state->hover.IsHovered(node);
    }
    [[nodiscard]] bool IsPressed(
        UINodeHandle node, Engine::PointerButton button = Engine::PointerButton::None) const {
        return state && state->press.IsPressed(node, button);
    }
    [[nodiscard]] bool HasPointerCapture(UINodeHandle node) const {
        return state && state->capture.HasPointerCapture(node);
    }
    [[nodiscard]] bool IsFocused(UINodeHandle node) const {
        return focus && focus->IsFocused(node);
    }
    [[nodiscard]] bool HasFocusWithin(UINodeHandle node) const {
        return focus && focus->HasFocusWithin(node);
    }
    [[nodiscard]] bool IsWindowFocused() const { return focus && focus->IsWindowFocused(); }
    [[nodiscard]] std::uint64_t Revision() const {
        const std::uint64_t interaction = state ? state->revision : 0;
        const std::uint64_t window = IsWindowFocused() ? 0x51ed2705ull : 0xa17c9e2bull;
        return interaction ^ (focusRevision + window + 0x9e3779b97f4a7c15ull +
            (interaction << 6) + (interaction >> 2));
    }
};

} // namespace Engine::UI2D
