#pragma once

#include "Platform/IApplicationHost.h"

namespace Engine {

class GlfwApplicationHost final : public IApplicationHost {
public:
    GlfwApplicationHost();
    ~GlfwApplicationHost() override;

    GlfwApplicationHost(const GlfwApplicationHost&) = delete;
    GlfwApplicationHost& operator=(const GlfwApplicationHost&) = delete;

    bool IsInitialized() const;

    void PollEvents() override;
    void WaitForEvents() override;
    void WaitForEventsTimeout(double seconds) override;
    bool ShouldExit() const override;
    void RequestExit() override;

private:
    bool m_Initialized = false;
    bool m_ShouldExit = false;
};

} // namespace Engine
