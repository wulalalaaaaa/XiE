#include "WindowPlacementUtils.h"

#include <algorithm>
#include <cmath>

namespace Engine {

namespace {

int RoundToInt(float value) {
    return static_cast<int>(std::lround(value));
}

} // namespace

PointI ScalePointForDpi(PointI point, float dpiScale) {
    const float scale = dpiScale > 0.0f ? dpiScale : 1.0f;
    return {RoundToInt(static_cast<float>(point.x) * scale), RoundToInt(static_cast<float>(point.y) * scale)};
}

PointI UnscalePointForDpi(PointI point, float dpiScale) {
    const float scale = dpiScale > 0.0f ? dpiScale : 1.0f;
    return {RoundToInt(static_cast<float>(point.x) / scale), RoundToInt(static_cast<float>(point.y) / scale)};
}

PointI ClampWindowPositionToWorkArea(PointI desiredPosition, int width, int height, RectI workArea) {
    const int maxX = workArea.x + std::max(0, workArea.width - width);
    const int maxY = workArea.y + std::max(0, workArea.height - height);
    return {
        std::clamp(desiredPosition.x, workArea.x, maxX),
        std::clamp(desiredPosition.y, workArea.y, maxY)
    };
}

} // namespace Engine
