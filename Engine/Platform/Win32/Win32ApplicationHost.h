#pragma once

#include "Platform/IApplicationHost.h"

namespace Engine {

class Win32ApplicationHost final : public IApplicationHost {
public:
    Win32ApplicationHost();

    void PollEvents() override;
    void WaitForEvents() override;
    void WaitForEventsTimeout(double seconds) override;
    void WakeEventLoop() override;
    bool ShouldExit() const override;
    void RequestExit() override;

private:
    bool m_ShouldExit = false;
    unsigned long m_ThreadId = 0;
};

} // namespace Engine
