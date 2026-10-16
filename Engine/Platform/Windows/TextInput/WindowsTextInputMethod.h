#pragma once

#include "Platform/TextInput/ITextInputMethod.h"

#include <functional>

namespace Engine {

class IWindowSystem;

class WindowsTextInputMethod final : public ITextInputMethod {
public:
    explicit WindowsTextInputMethod(IWindowSystem& windows)
        : m_Windows(&windows) {}
    explicit WindowsTextInputMethod(
        std::function<IWindowSystem*(WindowHandle)> resolver)
        : m_Resolver(std::move(resolver)) {}

    void Activate(WindowHandle window) override;
    void Deactivate(WindowHandle window) override;
    void SetCaretRect(
        WindowHandle window, const RectF& screenPhysicalRect) override;
    void CancelComposition(WindowHandle window) override;

private:
    IWindowSystem* Resolve(WindowHandle window) const;
    IWindowSystem* m_Windows = nullptr;
    std::function<IWindowSystem*(WindowHandle)> m_Resolver;
};

} // namespace Engine
