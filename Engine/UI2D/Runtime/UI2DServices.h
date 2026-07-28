#pragma once

namespace Engine::UI2D {

class IUILayoutEngine;
class IUIHitTester;
class IUIInputRouter;
class IUIRenderBuilder;

// The runtime owns window-local focus and animation state. Injected services are
// stateless or explicitly window-local (input router).
struct UI2DServices {
    IUILayoutEngine& layoutEngine;
    IUIHitTester& hitTester;
    IUIInputRouter& inputRouter;
    IUIRenderBuilder& renderBuilder;
};

} // namespace Engine::UI2D
