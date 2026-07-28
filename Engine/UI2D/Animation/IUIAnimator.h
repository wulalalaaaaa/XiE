#pragma once

#include "UI2D/Animation/UIAnimationTypes.h"

namespace Engine::UI2D {

class UIScene;

class IUIAnimator {
public:
    virtual ~IUIAnimator() = default;
    virtual UIAnimationStartResult Start(UIScene& scene, const UIAnimationDesc& desc) = 0;
    virtual bool Pause(UIAnimationHandle handle) = 0;
    virtual bool Resume(UIAnimationHandle handle) = 0;
    virtual bool Cancel(UIScene& scene, UIAnimationHandle handle, UIAnimationCancelMode mode) = 0;
    virtual std::uint32_t CancelAllForNode(
        UIScene& scene, UINodeHandle node, UIAnimationCancelMode mode) = 0;
    virtual std::uint32_t CancelAllForNodeChannel(UIScene&, UINodeHandle,
        UIAnimationChannel, UIAnimationCancelMode) { return 0; }
    virtual std::uint32_t CancelAll(UIScene& scene, UIAnimationCancelMode mode) = 0;
    virtual std::uint32_t CancelAllForChannel(
        UIScene&, UIAnimationChannel, UIAnimationCancelMode) { return 0; }
    virtual UIAnimationUpdateResult Update(UIScene& scene, double deltaSeconds) = 0;
    [[nodiscard]] virtual bool HasActiveAnimations() const = 0;
    [[nodiscard]] virtual bool IsRunning(UIAnimationHandle handle) const = 0;
    virtual void OnNodeInvalidated(UIScene& scene, UINodeHandle node) = 0;
};

class NullUIAnimator final : public IUIAnimator {
public:
    UIAnimationStartResult Start(UIScene&, const UIAnimationDesc&) override { return {}; }
    bool Pause(UIAnimationHandle) override { return false; }
    bool Resume(UIAnimationHandle) override { return false; }
    bool Cancel(UIScene&, UIAnimationHandle, UIAnimationCancelMode) override { return false; }
    std::uint32_t CancelAllForNode(UIScene&, UINodeHandle, UIAnimationCancelMode) override { return 0; }
    std::uint32_t CancelAllForNodeChannel(
        UIScene&, UINodeHandle, UIAnimationChannel, UIAnimationCancelMode) override { return 0; }
    std::uint32_t CancelAll(UIScene&, UIAnimationCancelMode) override { return 0; }
    std::uint32_t CancelAllForChannel(UIScene&, UIAnimationChannel, UIAnimationCancelMode) override { return 0; }
    UIAnimationUpdateResult Update(UIScene&, double) override { return {}; }
    bool HasActiveAnimations() const override { return false; }
    bool IsRunning(UIAnimationHandle) const override { return false; }
    void OnNodeInvalidated(UIScene&, UINodeHandle) override {}
};

} // namespace Engine::UI2D
