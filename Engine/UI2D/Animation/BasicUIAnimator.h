#pragma once

#include "UI2D/Animation/IUIAnimator.h"

#include <deque>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Engine::UI2D {

class BasicUIAnimator final : public IUIAnimator {
public:
    BasicUIAnimator();

    UIAnimationStartResult Start(UIScene& scene, const UIAnimationDesc& desc) override;
    bool Pause(UIAnimationHandle handle) override;
    bool Resume(UIAnimationHandle handle) override;
    bool Cancel(UIScene& scene, UIAnimationHandle handle, UIAnimationCancelMode mode) override;
    std::uint32_t CancelAllForNode(
        UIScene& scene, UINodeHandle node, UIAnimationCancelMode mode) override;
    std::uint32_t CancelAllForNodeChannel(UIScene& scene, UINodeHandle node,
        UIAnimationChannel channel, UIAnimationCancelMode mode) override;
    std::uint32_t CancelAll(UIScene& scene, UIAnimationCancelMode mode) override;
    std::uint32_t CancelAllForChannel(
        UIScene& scene, UIAnimationChannel channel, UIAnimationCancelMode mode) override;
    UIAnimationUpdateResult Update(UIScene& scene, double deltaSeconds) override;
    [[nodiscard]] bool HasActiveAnimations() const override {
        return !m_Active.empty() || !m_PendingStarts.empty();
    }
    [[nodiscard]] bool IsRunning(UIAnimationHandle handle) const override;
    void OnNodeInvalidated(UIScene& scene, UINodeHandle node) override;
    [[nodiscard]] std::uint32_t ActiveAnimationCount() const noexcept {
        return static_cast<std::uint32_t>(m_Active.size() + m_PendingStarts.size());
    }

private:
    struct ChannelKey {
        UINodeHandle node{};
        UIAnimatedProperty property = UIAnimatedProperty::Opacity;
        UIAnimationChannel channel = UIAnimationChannel::Application;
        friend bool operator==(const ChannelKey&, const ChannelKey&) = default;
    };
    struct ChannelHash {
        std::size_t operator()(const ChannelKey& key) const noexcept;
    };
    struct Record {
        UIAnimationHandle handle{};
        UIAnimationDesc desc{};
        UIAnimationValue resolvedFrom = 0.0f;
        UIAnimationValue resolvedTo = 1.0f;
        UIAnimationPlaybackState state = UIAnimationPlaybackState::Queued;
        double elapsedSeconds = 0.0;
        std::uint64_t creationOrder = 0;
        bool pendingActivation = false;
    };
    struct Slot {
        std::uint32_t generation = 0;
        std::optional<Record> record;
    };
    struct Channel {
        UIAnimationHandle current{};
        std::deque<UIAnimationHandle> queued;
    };
    struct DeferredCallback {
        UIAnimationCompletionCallback completion;
        UIAnimationCancelCallback cancel;
        UIAnimationHandle handle{};
        UINodeHandle node{};
        bool canceled = false;
    };

    UIAnimationHandle Allocate(const UIAnimationDesc& desc);
    Record* TryGet(UIAnimationHandle handle);
    const Record* TryGet(UIAnimationHandle handle) const;
    UIAnimationStartResult ActivateRequest(
        UIScene& scene, UIAnimationHandle handle, std::optional<UIAnimationValue> replaceCurrent = {});
    void ActivateRecord(UIScene& scene, Record& record, std::optional<UIAnimationValue> current = {});
    bool Apply(UIScene& scene, Record& record, const UIAnimationValue& value,
        UIAnimationUpdateResult* result = nullptr);
    bool CancelNow(UIScene& scene, UIAnimationHandle handle, UIAnimationCancelMode mode,
        bool invokeCancel, std::vector<DeferredCallback>* callbacks = nullptr);
    void CompleteNow(UIScene& scene, UIAnimationHandle handle,
        UIAnimationUpdateResult& result, std::vector<DeferredCallback>& callbacks);
    void Release(UIAnimationHandle handle);
    void RemoveActive(UIAnimationHandle handle);
    void StartNext(UIScene& scene, const ChannelKey& key);
    static float DirectedProgress(UIAnimationDirection direction, std::uint64_t iteration, float t) noexcept;

    std::vector<Slot> m_Slots{1};
    std::vector<std::uint32_t> m_Free;
    std::vector<UIAnimationHandle> m_Active;
    std::unordered_map<ChannelKey, Channel, ChannelHash> m_Channels;
    std::vector<UIAnimationHandle> m_UpdateSnapshot;
    std::vector<DeferredCallback> m_CallbackScratch;
    std::vector<UIAnimationHandle> m_PendingStarts;
    std::vector<std::pair<UIAnimationHandle, UIAnimationCancelMode>> m_PendingCancels;
    std::uint64_t m_NextCreationOrder = 1;
    bool m_InUpdate = false;
    bool m_InCallback = false;
};

} // namespace Engine::UI2D
