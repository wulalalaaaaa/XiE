#include "GlfwApplicationHost.h"

#include <GLFW/glfw3.h>

namespace Engine {

GlfwApplicationHost::GlfwApplicationHost() {
    m_Initialized = glfwInit() == GLFW_TRUE;
}

GlfwApplicationHost::~GlfwApplicationHost() {
    if (m_Initialized) {
        glfwTerminate();
        m_Initialized = false;
    }
}

bool GlfwApplicationHost::IsInitialized() const {
    return m_Initialized;
}

void GlfwApplicationHost::PollEvents() {
    if (m_Initialized) {
        glfwPollEvents();
    }
}

void GlfwApplicationHost::WaitForEvents() {
    if (m_Initialized) {
        glfwWaitEvents();
    }
}

void GlfwApplicationHost::WaitForEventsTimeout(double seconds) {
    if (m_Initialized) {
        glfwWaitEventsTimeout(seconds);
    }
}

bool GlfwApplicationHost::ShouldExit() const {
    return m_ShouldExit;
}

void GlfwApplicationHost::RequestExit() {
    m_ShouldExit = true;
    if (m_Initialized) {
        glfwPostEmptyEvent();
    }
}

} // namespace Engine
