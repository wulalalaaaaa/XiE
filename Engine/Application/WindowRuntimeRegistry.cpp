#include "Application/WindowRuntimeRegistry.h"

namespace Engine {

WindowHandle WindowRuntimeRegistry::Register(WindowContext context) {
    if (!context.window.IsValid()) {
        return {};
    }
    if (context.window.index > m_Slots.size()) {
        m_Slots.resize(context.window.index);
    }
    Slot& slot = m_Slots[context.window.index - 1];
    if (slot.context.has_value()) {
        return {};
    }
    if (slot.generation != 0 && context.window.generation < slot.generation) {
        return {};
    }
    slot.generation = context.window.generation;
    slot.context.emplace(std::move(context));
    ++m_Size;
    return slot.context->window;
}

bool WindowRuntimeRegistry::Unregister(WindowHandle handle) {
    if (TryGet(handle) == nullptr) {
        return false;
    }
    Slot& slot = m_Slots[handle.index - 1];
    slot.context.reset();
    ++slot.generation;
    if (slot.generation == 0) {
        slot.generation = 1;
    }
    --m_Size;
    return true;
}

WindowContext* WindowRuntimeRegistry::TryGet(WindowHandle handle) {
    return const_cast<WindowContext*>(static_cast<const WindowRuntimeRegistry*>(this)->TryGet(handle));
}

const WindowContext* WindowRuntimeRegistry::TryGet(WindowHandle handle) const {
    if (!handle.IsValid() || handle.index > m_Slots.size()) {
        return nullptr;
    }
    const Slot& slot = m_Slots[handle.index - 1];
    if (!slot.context.has_value() || slot.generation != handle.generation) {
        return nullptr;
    }
    return &*slot.context;
}

void WindowRuntimeRegistry::ForEachActiveWindow(const std::function<void(WindowContext&)>& visitor) {
    for (Slot& slot : m_Slots) {
        if (slot.context.has_value()) {
            visitor(*slot.context);
        }
    }
}

} // namespace Engine
