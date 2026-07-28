#include "Application/ApplicationCommandQueue.h"

#include "Application/ApplicationRuntime.h"

namespace Engine {

bool ApplicationCommandQueue::Enqueue(std::unique_ptr<IApplicationCommand> command) {
    if (!command) return false;
    std::scoped_lock lock(m_Mutex);
    if (m_Closed) return false;
    m_Pending.push_back(std::move(command));
    return true;
}

std::size_t ApplicationCommandQueue::ExecutePending(ApplicationRuntime& runtime) {
    std::vector<std::unique_ptr<IApplicationCommand>> pending;
    {
        std::scoped_lock lock(m_Mutex);
        pending.swap(m_Pending);
    }
    for (auto& command : pending) command->Execute(runtime);
    return pending.size();
}

void ApplicationCommandQueue::Close() {
    std::scoped_lock lock(m_Mutex);
    m_Closed = true;
    m_Pending.clear();
}

bool ApplicationCommandQueue::Empty() const {
    std::scoped_lock lock(m_Mutex);
    return m_Pending.empty();
}

} // namespace Engine
