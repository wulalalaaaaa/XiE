#include "DrawListValidator.h"

#include <cmath>
#include <type_traits>

namespace Engine {
namespace {

bool Finite(float value) { return std::isfinite(value); }
bool Finite(Vec2F value) { return Finite(value.x) && Finite(value.y); }
bool Finite(RectF value) { return Finite(value.x) && Finite(value.y) && Finite(value.width) && Finite(value.height); }
bool Finite(Color4f value) { return Finite(value.r) && Finite(value.g) && Finite(value.b) && Finite(value.a); }
bool Finite(const Mat3F& value) {
    return Finite(value.m00)&&Finite(value.m01)&&Finite(value.m02)&&Finite(value.m10)&&Finite(value.m11)&&
        Finite(value.m12)&&Finite(value.m20)&&Finite(value.m21)&&Finite(value.m22);
}
bool ValidColor(Color4f value) {
    return Finite(value) && value.r>=0&&value.r<=1&&value.g>=0&&value.g<=1&&
        value.b>=0&&value.b<=1&&value.a>=0&&value.a<=1;
}

} // namespace

DrawListValidationResult DrawListValidator::Validate(const DrawList2D& drawList) const {
    DrawListValidationResult result{};
    int depth = 0;

    for (const DrawCommand2D& command : drawList.Commands()) {
        if (std::holds_alternative<PushClipRectCommand>(command)) {
            const RectF rect = std::get<PushClipRectCommand>(command).rect;
            if (!Finite(rect) || rect.width < 0 || rect.height < 0) {
                result.valid = false;
                result.errors.emplace_back("Invalid clip rectangle");
            }
            ++depth;
            continue;
        }
        if (std::holds_alternative<PopClipCommand>(command)) {
            --depth;
            if (depth < 0) {
                result.valid = false;
                result.errors.emplace_back("DrawList2D clip stack underflow");
                depth = 0;
            }
            continue;
        }
        std::visit([&](const auto& typed) {
            using T = std::decay_t<decltype(typed)>;
            const auto fail = [&](const char* message) { result.valid=false; result.errors.emplace_back(message); };
            if constexpr (std::is_same_v<T, PushClipRectCommand>) {
                if (!Finite(typed.rect) || typed.rect.width < 0 || typed.rect.height < 0) fail("Invalid clip rectangle");
            } else if constexpr (!std::is_same_v<T, PopClipCommand>) {
                if constexpr (requires { typed.transform; }) if (!Finite(typed.transform)) fail("Invalid draw transform");
                if constexpr (requires { typed.color; }) if (!ValidColor(typed.color)) fail("Invalid draw color");
                if constexpr (requires { typed.rect; }) if (!Finite(typed.rect) || typed.rect.width<0 || typed.rect.height<0) fail("Invalid draw rectangle");
                if constexpr (requires { typed.dst; }) if (!Finite(typed.dst) || typed.dst.width<0 || typed.dst.height<0) fail("Invalid destination rectangle");
                if constexpr (std::is_same_v<T, SpriteCommand> || std::is_same_v<T, NineSliceCommand>)
                    if (!typed.texture.IsValid()) fail("Invalid texture handle");
                if constexpr (std::is_same_v<T, RoundedRectCommand>)
                    if (!Finite(typed.radius) || typed.radius<0) fail("Invalid rounded rectangle radius");
                if constexpr (std::is_same_v<T, CircleCommand>)
                    if (!Finite(typed.center)||!Finite(typed.radius)||typed.radius<0) fail("Invalid circle");
                if constexpr (std::is_same_v<T, RingCommand>)
                    if (!Finite(typed.center)||!Finite(typed.radius)||!Finite(typed.thickness)||typed.radius<0||typed.thickness<0||typed.thickness>typed.radius) fail("Invalid ring");
                if constexpr (std::is_same_v<T, LineCommand>)
                    if (!Finite(typed.from)||!Finite(typed.to)||!Finite(typed.thickness)||typed.thickness<=0) fail("Invalid line");
                if constexpr (std::is_same_v<T, TextCommand>)
                    if (!typed.layout.IsValid()||!Finite(typed.position)) fail("Invalid text command");
            }
        }, command);
    }

    result.clipDepth = depth;
    if (depth != 0) {
        result.valid = false;
        result.errors.emplace_back("DrawList2D clip stack not balanced");
    }
    return result;
}

} // namespace Engine
