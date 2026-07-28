#pragma once

#include "Platform/IApplicationHost.h"

namespace Engine {

class Win32ApplicationHost final : public IApplicationHost {
public:
    Win32ApplicationHost();

    void PollEvents() override;
    void WaitForEvents() override;
    void WaitForEventsTimeout(double seconds) override;
    bool ShouldExit() const override;
    void RequestExit() override;

private:
    bool m_ShouldExit = false;
};

} // namespace Engine
