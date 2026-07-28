#pragma once

namespace Engine {

class IApplicationHost;
class IWindowSystem;

struct ApplicationServices {
    IApplicationHost& host;
    IWindowSystem& windows;
};

} // namespace Engine
