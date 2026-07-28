#include "UI2D/Animation/BasicUIAnimator.h"

#include "UI2D/Animation/UIAnimatedProperties.h"
#include "UI2D/Core/UIScene.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>

namespace Engine::UI2D {
namespace {

std::atomic<std::uint32_t> g_NextAnimationGeneration{1};

std::uint32_t NextAnimationGeneration() noexcept {
    std::uint32_t generation = g_NextAnimationGeneration.fetch_add(1, std::memory_order_relaxed);
    while (generation == 0)
        generation = g_NextAnimationGeneration.fetch_add(1, std::memory_order_relaxed);
    return generation;
}

bool Finite(const UIAnimationValue& value) {
    if (const float* scalar = std::get_if<float>(&value)) return std::isfinite(*scalar);
    if (const Engine::Vec2F* vector = std::get_if<Engine::Vec2F>(&value))
        return std::isfinite(vector->x) && std::isfinite(vector->y);
    const Engine::Color4f color = std::get<Engine::Color4f>(value);
    return std::isfinite(color.r) && std::isfinite(color.g) && std::isfinite(color.b) && std::isfinite(color.a);
}

bool HasBackwardsFill(UIAnimationFillMode mode) {
    return mode == UIAnimationFillMode::Backwards || mode == UIAnimationFillMode::Both;
}
bool HasForwardsFill(UIAnimationFillMode mode) {
    return mode == UIAnimationFillMode::Forwards || mode == UIAnimationFillMode::Both;
}

} // namespace

BasicUIAnimator::BasicUIAnimator() = default;

std::size_t BasicUIAnimator::ChannelHash::operator()(const ChannelKey& key) const noexcept {
    std::size_t value = key.node.index;
    value = value * 16777619u ^ key.node.generation;
    value = value * 31u ^ static_cast<std::size_t>(key.property);
    return value * 31u ^ static_cast<std::size_t>(key.channel);
}

UIAnimationHandle BasicUIAnimator::Allocate(const UIAnimationDesc& desc) {
    std::uint32_t index = 0;
    if (!m_Free.empty()) {
        index = m_Free.back();
        m_Free.pop_back();
    } else {
        index = static_cast<std::uint32_t>(m_Slots.size());
        m_Slots.push_back({});
    }
    Slot& slot = m_Slots[index];
    slot.generation = NextAnimationGeneration();
    const UIAnimationHandle handle{index, slot.generation};
    slot.record = Record{handle, desc, desc.from, desc.to, UIAnimationPlaybackState::Queued,
        0.0, m_NextCreationOrder++, false};
    return handle;
}

BasicUIAnimator::Record* BasicUIAnimator::TryGet(UIAnimationHandle handle) {
    if (!handle.IsValid() || handle.index >= m_Slots.size()) return nullptr;
    Slot& slot = m_Slots[handle.index];
    return slot.generation == handle.generation && slot.record ? &*slot.record : nullptr;
}
const BasicUIAnimator::Record* BasicUIAnimator::TryGet(UIAnimationHandle handle) const {
    if (!handle.IsValid() || handle.index >= m_Slots.size()) return nullptr;
    const Slot& slot = m_Slots[handle.index];
    return slot.generation == handle.generation && slot.record ? &*slot.record : nullptr;
}

UIAnimationStartResult BasicUIAnimator::Start(UIScene& scene, const UIAnimationDesc& input) {
    if (!scene.TryGet(input.node)) return {false, {}, "animation node is invalid"};
    if (input.channel == UIAnimationChannel::InteractionStyle &&
        input.property == UIAnimatedProperty::VisualSize)
        return {false, {}, "interaction style cannot animate visual size"};
    if (!IsAnimationValueCompatible(input.property, input.to) ||
        (input.fromMode == UIAnimationFromMode::Explicit && !IsAnimationValueCompatible(input.property, input.from)))
        return {false, {}, "animation value type does not match property"};
    if (!Finite(input.to) || (input.fromMode == UIAnimationFromMode::Explicit && !Finite(input.from)))
        return {false, {}, "animation value must be finite"};
    if (!std::isfinite(input.durationSeconds) || input.durationSeconds < 0.0)
        return {false, {}, "duration must be finite and non-negative"};
    if (input.infinite && input.durationSeconds == 0.0)
        return {false, {}, "an infinite animation requires positive duration"};
    if (input.iterationCount == 0 && !input.infinite)
        return {false, {}, "iterationCount must be at least one"};

    UIAnimationDesc desc = input;
    if (!std::isfinite(desc.delaySeconds) || desc.delaySeconds < 0.0) desc.delaySeconds = 0.0;
    if ((m_InUpdate || m_InCallback) && desc.replaceMode == UIAnimationReplaceMode::Reject) {
        const auto channel = m_Channels.find({desc.node, desc.property, desc.channel});
        if (channel != m_Channels.end() && channel->second.current.IsValid())
            return {false, {}, "property channel is already active"};
    }
    const UIAnimationHandle handle = Allocate(desc);
    if (m_InUpdate || m_InCallback) {
        TryGet(handle)->pendingActivation = true;
        m_PendingStarts.push_back(handle);
        return {true, handle, {}};
    }
    return ActivateRequest(scene, handle);
}

UIAnimationStartResult BasicUIAnimator::ActivateRequest(
    UIScene& scene, UIAnimationHandle handle, std::optional<UIAnimationValue> replaceCurrent) {
    Record* record = TryGet(handle);
    if (!record || !scene.TryGet(record->desc.node)) {
        Release(handle);
        return {false, {}, "animation node is invalid"};
    }
    record->pendingActivation = false;
    const ChannelKey key{record->desc.node, record->desc.property, record->desc.channel};
    Channel& channel = m_Channels[key];
    if (channel.current.IsValid()) {
        if (record->desc.replaceMode == UIAnimationReplaceMode::Reject) {
            Release(handle);
            return {false, {}, "property channel is already active"};
        }
        if (record->desc.replaceMode == UIAnimationReplaceMode::Queue) {
            record->state = UIAnimationPlaybackState::Queued;
            channel.queued.push_back(handle);
            return {true, handle, {}};
        }

        const UINodeRecord* node = scene.TryGet(record->desc.node);
        const UIAnimationValue current = replaceCurrent.value_or(ResolveAnimationChannelProperty(
            *node, record->desc.property, record->desc.channel));
        std::vector<DeferredCallback> cancelCallbacks;
        const std::deque<UIAnimationHandle> queued = std::move(channel.queued);
        channel.queued.clear();
        for (UIAnimationHandle queuedHandle : queued)
            (void)CancelNow(scene, queuedHandle, UIAnimationCancelMode::RestoreBase, true, &cancelCallbacks);
        const UIAnimationHandle previous = channel.current;
        (void)CancelNow(scene, previous, UIAnimationCancelMode::RestoreBase, true, &cancelCallbacks);
        Channel& fresh = m_Channels[key];
        fresh.current = handle;
        ActivateRecord(scene, *TryGet(handle), current);
        m_InCallback = true;
        for (const DeferredCallback& callback : cancelCallbacks) callback.cancel(callback.handle, callback.node);
        m_InCallback = false;
        return {true, handle, {}};
    }
    channel.current = handle;
    ActivateRecord(scene, *record);
    return {true, handle, {}};
}

void BasicUIAnimator::ActivateRecord(
    UIScene& scene, Record& record, std::optional<UIAnimationValue> current) {
    const UINodeRecord* node = scene.TryGet(record.desc.node);
    if (record.desc.fromMode == UIAnimationFromMode::Current)
        record.resolvedFrom = current.value_or(ResolveAnimationChannelProperty(
            *node, record.desc.property, record.desc.channel));
    else record.resolvedFrom = record.desc.from;
    record.resolvedTo = record.desc.to;
    record.elapsedSeconds = 0.0;
    record.state = record.desc.delaySeconds > 0.0
        ? UIAnimationPlaybackState::Delayed : UIAnimationPlaybackState::Running;
    m_Active.push_back(record.handle);
    if (record.desc.delaySeconds <= 0.0 || HasBackwardsFill(record.desc.fillMode)) {
        const float p = DirectedProgress(record.desc.direction, 0, 0.0f);
        (void)Apply(scene, record, InterpolateAnimationValue(record.resolvedFrom, record.resolvedTo,
            EvaluateEasing(record.desc.easing, p)));
    }
}

bool BasicUIAnimator::Apply(UIScene& scene, Record& record, const UIAnimationValue& value,
    UIAnimationUpdateResult* result) {
    const bool changed = SetAnimationChannelValue(
        scene, record.desc.node, record.desc.property, record.desc.channel, value);
    if (!changed || !result) return changed;
    const UIDirtyFlags flags = AnimatedPropertyDirtyFlags(record.desc.property);
    result->transformInvalidated |= HasAny(flags, UIDirtyFlags::Transform);
    result->visualInvalidated |= HasAny(flags, UIDirtyFlags::Visual);
    result->hitTestInvalidated |= HasAny(flags, UIDirtyFlags::HitTest);
    ++result->updatedCount;
    return true;
}

float BasicUIAnimator::DirectedProgress(
    UIAnimationDirection direction, std::uint64_t iteration, float t) noexcept {
    const bool odd = (iteration & 1u) != 0;
    switch (direction) {
    case UIAnimationDirection::Reverse: return 1.0f - t;
    case UIAnimationDirection::Alternate: return odd ? 1.0f - t : t;
    case UIAnimationDirection::AlternateReverse: return odd ? t : 1.0f - t;
    case UIAnimationDirection::Normal: default: return t;
    }
}

bool BasicUIAnimator::Pause(UIAnimationHandle handle) {
    Record* record = TryGet(handle);
    if (!record || (record->state != UIAnimationPlaybackState::Running &&
        record->state != UIAnimationPlaybackState::Delayed)) return false;
    record->state = UIAnimationPlaybackState::Paused;
    RemoveActive(handle);
    return true;
}

bool BasicUIAnimator::Resume(UIAnimationHandle handle) {
    Record* record = TryGet(handle);
    if (!record || record->state != UIAnimationPlaybackState::Paused) return false;
    record->state = record->elapsedSeconds < record->desc.delaySeconds
        ? UIAnimationPlaybackState::Delayed : UIAnimationPlaybackState::Running;
    m_Active.push_back(handle);
    return true;
}

bool BasicUIAnimator::IsRunning(UIAnimationHandle handle) const {
    const Record* record = TryGet(handle);
    return record && (record->state == UIAnimationPlaybackState::Running ||
        record->state == UIAnimationPlaybackState::Delayed);
}

void BasicUIAnimator::RemoveActive(UIAnimationHandle handle) {
    std::erase(m_Active, handle);
}

void BasicUIAnimator::Release(UIAnimationHandle handle) {
    if (!handle.IsValid() || handle.index >= m_Slots.size()) return;
    Slot& slot = m_Slots[handle.index];
    if (slot.generation != handle.generation || !slot.record) return;
    slot.record.reset();
    m_Free.push_back(handle.index);
}

void BasicUIAnimator::StartNext(UIScene& scene, const ChannelKey& key) {
    auto it = m_Channels.find(key);
    if (it == m_Channels.end()) return;
    Channel& channel = it->second;
    channel.current = {};
    while (!channel.queued.empty()) {
        const UIAnimationHandle next = channel.queued.front();
        channel.queued.pop_front();
        Record* record = TryGet(next);
        if (!record || !scene.TryGet(record->desc.node)) {
            Release(next);
            continue;
        }
        channel.current = next;
        ActivateRecord(scene, *record);
        return;
    }
    m_Channels.erase(it);
}

bool BasicUIAnimator::CancelNow(UIScene& scene, UIAnimationHandle handle,
    UIAnimationCancelMode mode, bool invokeCancel, std::vector<DeferredCallback>* callbacks) {
    Record* record = TryGet(handle);
    if (!record) return false;
    const ChannelKey key{record->desc.node, record->desc.property, record->desc.channel};
    const UINodeHandle node = record->desc.node;
    const UIAnimationCancelCallback callback = record->desc.onCanceled;
    auto channelIt = m_Channels.find(key);
    const bool current = channelIt != m_Channels.end() && channelIt->second.current == handle;
    if (channelIt != m_Channels.end() && !current) std::erase(channelIt->second.queued, handle);
    RemoveActive(handle);
    if (current && mode == UIAnimationCancelMode::RestoreBase)
        (void)ClearAnimationChannelValue(scene, node, record->desc.property, record->desc.channel);
    record->state = UIAnimationPlaybackState::Canceled;
    Release(handle);
    if (current) StartNext(scene, key);
    if (invokeCancel && callback) {
        if (callbacks) callbacks->push_back({{}, callback, handle, node, true});
        else {
            m_InCallback = true;
            callback(handle, node);
            m_InCallback = false;
        }
    }
    return true;
}

bool BasicUIAnimator::Cancel(
    UIScene& scene, UIAnimationHandle handle, UIAnimationCancelMode mode) {
    if (m_InUpdate || m_InCallback) {
        if (!TryGet(handle)) return false;
        m_PendingCancels.emplace_back(handle, mode);
        return true;
    }
    return CancelNow(scene, handle, mode, true);
}

std::uint32_t BasicUIAnimator::CancelAllForNode(
    UIScene& scene, UINodeHandle node, UIAnimationCancelMode mode) {
    std::vector<UIAnimationHandle> handles;
    for (std::size_t i = 1; i < m_Slots.size(); ++i)
        if (m_Slots[i].record && m_Slots[i].record->desc.node == node) handles.push_back(m_Slots[i].record->handle);
    std::uint32_t count = 0;
    for (UIAnimationHandle handle : handles) count += Cancel(scene, handle, mode) ? 1u : 0u;
    return count;
}

std::uint32_t BasicUIAnimator::CancelAllForNodeChannel(UIScene& scene, UINodeHandle node,
    UIAnimationChannel channel, UIAnimationCancelMode mode) {
    std::vector<UIAnimationHandle> handles;
    for (std::size_t i = 1; i < m_Slots.size(); ++i) {
        if (m_Slots[i].record && m_Slots[i].record->desc.node == node &&
            m_Slots[i].record->desc.channel == channel)
            handles.push_back(m_Slots[i].record->handle);
    }
    std::uint32_t count = 0;
    for (UIAnimationHandle handle : handles) count += Cancel(scene, handle, mode) ? 1u : 0u;
    return count;
}

std::uint32_t BasicUIAnimator::CancelAll(UIScene& scene, UIAnimationCancelMode mode) {
    std::vector<UIAnimationHandle> handles;
    for (std::size_t i = 1; i < m_Slots.size(); ++i)
        if (m_Slots[i].record) handles.push_back(m_Slots[i].record->handle);
    std::uint32_t count = 0;
    for (UIAnimationHandle handle : handles) count += Cancel(scene, handle, mode) ? 1u : 0u;
    return count;
}

std::uint32_t BasicUIAnimator::CancelAllForChannel(
    UIScene& scene, UIAnimationChannel channel, UIAnimationCancelMode mode) {
    std::vector<UIAnimationHandle> handles;
    for (std::size_t i = 1; i < m_Slots.size(); ++i) {
        if (m_Slots[i].record && m_Slots[i].record->desc.channel == channel)
            handles.push_back(m_Slots[i].record->handle);
    }
    std::uint32_t count = 0;
    for (UIAnimationHandle handle : handles) count += Cancel(scene, handle, mode) ? 1u : 0u;
    return count;
}

void BasicUIAnimator::CompleteNow(UIScene& scene, UIAnimationHandle handle,
    UIAnimationUpdateResult& result, std::vector<DeferredCallback>& callbacks) {
    Record* record = TryGet(handle);
    if (!record) return;
    const ChannelKey key{record->desc.node, record->desc.property, record->desc.channel};
    const UINodeHandle node = record->desc.node;
    const UIAnimationCompletionCallback callback = record->desc.onCompleted;
    if (HasForwardsFill(record->desc.fillMode)) {
        const std::uint64_t last = record->desc.iterationCount > 0 ? record->desc.iterationCount - 1u : 0u;
        const float p = DirectedProgress(record->desc.direction, last, 1.0f);
        (void)Apply(scene, *record, InterpolateAnimationValue(record->resolvedFrom, record->resolvedTo,
            EvaluateEasing(record->desc.easing, p)), &result);
    } else if (ClearAnimationChannelValue(scene, node, record->desc.property, record->desc.channel)) {
        const UIDirtyFlags flags = AnimatedPropertyDirtyFlags(record->desc.property);
        result.transformInvalidated |= HasAny(flags, UIDirtyFlags::Transform);
        result.visualInvalidated = true;
        result.hitTestInvalidated |= HasAny(flags, UIDirtyFlags::HitTest);
    }
    RemoveActive(handle);
    record->state = UIAnimationPlaybackState::Completed;
    Release(handle);
    ++result.completedCount;
    StartNext(scene, key);
    if (callback) callbacks.push_back({callback, {}, handle, node, false});
}

UIAnimationUpdateResult BasicUIAnimator::Update(UIScene& scene, double deltaSeconds) {
    UIAnimationUpdateResult result;
    if (m_InUpdate) {
        result.reentrantUpdateRejected = true;
        result.hasActiveAnimations = HasActiveAnimations();
        return result;
    }
    if (m_Active.empty() && m_PendingStarts.empty() && m_PendingCancels.empty()) return result;
    m_InUpdate = true;
    const double delta = !std::isfinite(deltaSeconds) || deltaSeconds < 0.0
        ? 0.0 : std::min(deltaSeconds, 86400.0);
    m_UpdateSnapshot.assign(m_Active.begin(), m_Active.end());
    m_CallbackScratch.clear();
    if (m_CallbackScratch.capacity() < m_UpdateSnapshot.size())
        m_CallbackScratch.reserve(m_UpdateSnapshot.size());

    for (UIAnimationHandle handle : m_UpdateSnapshot) {
        Record* record = TryGet(handle);
        if (!record || record->state == UIAnimationPlaybackState::Paused ||
            record->state == UIAnimationPlaybackState::Queued) continue;
        ++result.visitedAnimationCount;
        if (!scene.TryGet(record->desc.node)) {
            (void)CancelNow(scene, handle, UIAnimationCancelMode::RestoreBase, false, &m_CallbackScratch);
            ++result.canceledCount;
            continue;
        }
        record->elapsedSeconds += delta;
        if (record->elapsedSeconds < record->desc.delaySeconds) continue;
        record->state = UIAnimationPlaybackState::Running;
        const double playTime = record->elapsedSeconds - record->desc.delaySeconds;
        const double duration = record->desc.durationSeconds;
        const double total = duration * static_cast<double>(record->desc.iterationCount);
        if (!record->desc.infinite && (duration == 0.0 || playTime >= total)) {
            CompleteNow(scene, handle, result, m_CallbackScratch);
            continue;
        }
        std::uint64_t iteration = 0;
        float local = 1.0f;
        if (duration > 0.0) {
            const double rawIteration = std::floor(playTime / duration);
            iteration = rawIteration >= static_cast<double>(std::numeric_limits<std::uint64_t>::max())
                ? std::numeric_limits<std::uint64_t>::max() : static_cast<std::uint64_t>(rawIteration);
            local = static_cast<float>((playTime - rawIteration * duration) / duration);
        }
        const float directed = DirectedProgress(record->desc.direction, iteration, local);
        (void)Apply(scene, *record, InterpolateAnimationValue(record->resolvedFrom, record->resolvedTo,
            EvaluateEasing(record->desc.easing, directed)), &result);
    }

    m_InCallback = true;
    for (const DeferredCallback& callback : m_CallbackScratch) {
        if (callback.canceled) callback.cancel(callback.handle, callback.node);
        else callback.completion(callback.handle, callback.node);
    }
    m_InCallback = false;
    m_CallbackScratch.clear();
    const auto pendingCancels = std::move(m_PendingCancels);
    m_PendingCancels.clear();
    for (const auto& [handle, mode] : pendingCancels)
        if (CancelNow(scene, handle, mode, true)) ++result.canceledCount;
    const auto pendingStarts = std::move(m_PendingStarts);
    m_PendingStarts.clear();
    for (UIAnimationHandle handle : pendingStarts) (void)ActivateRequest(scene, handle);
    m_InUpdate = false;
    result.hasActiveAnimations = HasActiveAnimations();
    return result;
}

void BasicUIAnimator::OnNodeInvalidated(UIScene& scene, UINodeHandle node) {
    std::vector<UIAnimationHandle> handles;
    for (std::size_t i = 1; i < m_Slots.size(); ++i)
        if (m_Slots[i].record && m_Slots[i].record->desc.node == node) handles.push_back(m_Slots[i].record->handle);
    for (UIAnimationHandle handle : handles)
        (void)CancelNow(scene, handle, UIAnimationCancelMode::RestoreBase, false);
    if (UINodeRecord* record = scene.TryGet(node)) record->animated = {};
}

} // namespace Engine::UI2D
