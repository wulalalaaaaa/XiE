#pragma once

namespace Engine {

struct PointI {
    int x = 0;
    int y = 0;
};

struct RectI {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

PointI ScalePointForDpi(PointI point, float dpiScale);
PointI UnscalePointForDpi(PointI point, float dpiScale);
PointI ClampWindowPositionToWorkArea(PointI desiredPosition, int width, int height, RectI workArea);

} // namespace Engine
