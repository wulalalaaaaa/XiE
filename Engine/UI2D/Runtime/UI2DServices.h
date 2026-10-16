#pragma once

namespace Engine::UI2D {

class IUILayoutEngine;
class IUIHitTester;
class IUIInputRouter;
class IUIRenderBuilder;

}

namespace Engine {
class ITextClipboard;
class ITextInputMethod;
class ITextLayoutService;
class IWindowSystem;
}

namespace Engine::UI2D {

// The runtime owns window-local focus and animation state. Injected services are
// stateless or explicitly window-local (input router).
struct UI2DServices {
    IUILayoutEngine& layoutEngine;
    IUIHitTester& hitTester;
    IUIInputRouter& inputRouter;
    IUIRenderBuilder& renderBuilder;
    Engine::ITextLayoutService* textLayouts = nullptr;
    Engine::ITextClipboard* textClipboard = nullptr;
    Engine::ITextInputMethod* textInputMethod = nullptr;
    Engine::IWindowSystem* windowSystem = nullptr;
};

} // namespace Engine::UI2D
