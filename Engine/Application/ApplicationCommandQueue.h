#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>

namespace Engine {

class ApplicationRuntime;

class IApplicationCommand {
public:
    virtual ~IApplicationCommand() = default;
    virtual void Execute(ApplicationRuntime& runtime) = 0;
};

class ApplicationCommandQueue {
public:
    bool Enqueue(std::unique_ptr<IApplicationCommand> command);
    std::size_t ExecutePending(ApplicationRuntime& runtime);
    void Close();
    [[nodiscard]] bool Empty() const;

private:
    mutable std::mutex m_Mutex;
    std::vector<std::unique_ptr<IApplicationCommand>> m_Pending;
    bool m_Closed = false;
};

} // namespace Engine
