#pragma once

#include "Foundation/Math/Types2D.h"

#include <cmath>

namespace Engine {

// Row-major affine 2D matrix. Points are treated as column vectors.
struct Mat3F {
    float m00 = 1.0f; float m01 = 0.0f; float m02 = 0.0f;
    float m10 = 0.0f; float m11 = 1.0f; float m12 = 0.0f;
    float m20 = 0.0f; float m21 = 0.0f; float m22 = 1.0f;

    friend constexpr bool operator==(Mat3F, Mat3F) = default;
};

constexpr Mat3F Multiply(const Mat3F& a, const Mat3F& b) noexcept {
    return {
        a.m00 * b.m00 + a.m01 * b.m10 + a.m02 * b.m20,
        a.m00 * b.m01 + a.m01 * b.m11 + a.m02 * b.m21,
        a.m00 * b.m02 + a.m01 * b.m12 + a.m02 * b.m22,
        a.m10 * b.m00 + a.m11 * b.m10 + a.m12 * b.m20,
        a.m10 * b.m01 + a.m11 * b.m11 + a.m12 * b.m21,
        a.m10 * b.m02 + a.m11 * b.m12 + a.m12 * b.m22,
        a.m20 * b.m00 + a.m21 * b.m10 + a.m22 * b.m20,
        a.m20 * b.m01 + a.m21 * b.m11 + a.m22 * b.m21,
        a.m20 * b.m02 + a.m21 * b.m12 + a.m22 * b.m22};
}

constexpr Mat3F TranslationMatrix(Vec2F value) noexcept {
    Mat3F result;
    result.m02 = value.x;
    result.m12 = value.y;
    return result;
}

constexpr Mat3F ScaleMatrix(Vec2F value) noexcept {
    Mat3F result;
    result.m00 = value.x;
    result.m11 = value.y;
    return result;
}

inline Mat3F RotationMatrix(float radians) noexcept {
    Mat3F result;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result.m00 = cosine;
    result.m01 = -sine;
    result.m10 = sine;
    result.m11 = cosine;
    return result;
}

constexpr Vec2F TransformPoint(const Mat3F& matrix, Vec2F point) noexcept {
    return {
        matrix.m00 * point.x + matrix.m01 * point.y + matrix.m02,
        matrix.m10 * point.x + matrix.m11 * point.y + matrix.m12};
}

inline bool TryInverseAffine(const Mat3F& matrix, Mat3F& inverse) noexcept {
    const float determinant = matrix.m00 * matrix.m11 - matrix.m01 * matrix.m10;
    if (!std::isfinite(determinant) || std::abs(determinant) <= 1.0e-7f) return false;
    const float reciprocal = 1.0f / determinant;
    inverse = {};
    inverse.m00 = matrix.m11 * reciprocal;
    inverse.m01 = -matrix.m01 * reciprocal;
    inverse.m10 = -matrix.m10 * reciprocal;
    inverse.m11 = matrix.m00 * reciprocal;
    inverse.m02 = -(inverse.m00 * matrix.m02 + inverse.m01 * matrix.m12);
    inverse.m12 = -(inverse.m10 * matrix.m02 + inverse.m11 * matrix.m12);
    return true;
}

} // namespace Engine
