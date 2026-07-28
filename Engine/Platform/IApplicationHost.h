#pragma once

namespace Engine {

class IApplicationHost {
public:
    virtual ~IApplicationHost() = default;

    virtual void PollEvents() = 0;
    virtual void WaitForEvents() = 0;
    virtual void WaitForEventsTimeout(double seconds) = 0;
    virtual bool ShouldExit() const = 0;
    virtual void RequestExit() = 0;
};

} // namespace Engine
