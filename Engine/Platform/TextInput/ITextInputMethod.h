#pragma once

#include "Foundation/Math/Types2D.h"
#include "Platform/WindowHandle.h"

namespace Engine {

class ITextInputMethod {
public:
    virtual ~ITextInputMethod() = default;
    virtual void Activate(WindowHandle window) = 0;
    virtual void Deactivate(WindowHandle window) = 0;
    virtual void SetCaretRect(
        WindowHandle window, const RectF& screenPhysicalRect) = 0;
    virtual void CancelComposition(WindowHandle window) = 0;
};

} // namespace Engine
