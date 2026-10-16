#pragma once

namespace Crystal {

// Crystal keeps its canvas renderer intentionally lightweight, but all new
// workspace chrome uses this small semantic palette so display modes and
// interaction states do not need to invent unrelated RGB values.
struct CrystalThemeTokens {
    float background[4] = {0.045f, 0.055f, 0.075f, 1.0f};
    float surface[4] = {0.075f, 0.090f, 0.130f, 0.98f};
    float elevated[4] = {0.070f, 0.110f, 0.170f, 0.98f};
    float foreground[4] = {0.92f, 0.96f, 1.0f, 1.0f};
    float muted[4] = {0.52f, 0.64f, 0.74f, 1.0f};
    float accent[4] = {0.28f, 0.68f, 0.92f, 1.0f};
    float success[4] = {0.30f, 0.86f, 0.58f, 1.0f};
    float warning[4] = {0.98f, 0.72f, 0.38f, 1.0f};
    float error[4] = {1.0f, 0.42f, 0.36f, 1.0f};
    float selection[4] = {1.0f, 0.72f, 0.42f, 1.0f};
    float hover[4] = {0.26f, 0.82f, 0.70f, 1.0f};
    float locked[4] = {1.0f, 0.48f, 0.45f, 1.0f};
    float stale[4] = {0.98f, 0.56f, 0.30f, 1.0f};
    float disabled[4] = {0.36f, 0.46f, 0.54f, 1.0f};
    float offline[4] = {0.72f, 0.36f, 0.32f, 1.0f};
    float spacing = 8.0f;
    float cornerRadius = 6.0f;
};

inline constexpr CrystalThemeTokens kCrystalTheme{};

} // namespace Crystal
