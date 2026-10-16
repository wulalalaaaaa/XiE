#pragma once

#include <memory>

namespace Engine {
class ITextClipboard;
class ITextInputMethod;
class ITextLayoutService;
class TextureRegistry2D;
}

namespace Engine::UI2D {
class IUILayoutEngine;
class IUIHitTester;
class IUIInputRouter;
class IUIRenderBuilder;
class ResolvedUITheme;
}

namespace Engine {

// Null service pointers request XiE's default implementation.  Non-null
// pointers are borrowed for the lifetime of the binding and must outlive the
// WindowSession, which makes advanced injection explicit without moving
// application-owned services into XiE.
struct UI2DBindingOptions {
    UI2D::IUILayoutEngine* layoutEngine = nullptr;
    UI2D::IUIHitTester* hitTester = nullptr;
    UI2D::IUIInputRouter* inputRouter = nullptr;
    UI2D::IUIRenderBuilder* renderBuilder = nullptr;
    ITextLayoutService* textLayouts = nullptr;
    ITextClipboard* textClipboard = nullptr;
    ITextInputMethod* textInputMethod = nullptr;
    TextureRegistry2D* textures = nullptr;
    std::shared_ptr<const UI2D::ResolvedUITheme> theme;
    bool attachInput = true;
    bool bindRenderResources = true;
};

} // namespace Engine
