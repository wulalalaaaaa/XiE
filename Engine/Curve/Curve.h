#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace Engine::Curve {

using CurvePointId = std::uint64_t;

enum class CurveInterpolation : std::uint8_t {
    Linear,
    Smooth
};

struct CurvePoint {
    CurvePointId id = 0;
    float x = 0.0f;
    float y = 0.0f;
    CurveInterpolation interpolation = CurveInterpolation::Smooth;
};

enum class CurveStatus : std::uint8_t {
    Ok,
    NonFiniteValue,
    DuplicateX,
    PointNotFound,
    InvalidLutSize,
    InvalidDomain
};

class CurveModel {
public:
    CurveStatus AddPoint(CurvePoint point);
    CurveStatus RemovePoint(CurvePointId id);
    CurveStatus MovePoint(CurvePointId id, float x, float y);
    void Clear() noexcept;

    [[nodiscard]] std::span<const CurvePoint> Points() const noexcept { return m_Points; }

private:
    CurveStatus SortAndValidate();
    std::vector<CurvePoint> m_Points;
};

class CurveSampler {
public:
    [[nodiscard]] static float Sample(
        std::span<const CurvePoint> points,
        float x) noexcept;
};

class CurveLUT {
public:
    CurveStatus Build(
        std::span<const CurvePoint> points,
        std::size_t sampleCount,
        float domainMinimum,
        float domainMaximum);

    [[nodiscard]] float Sample(float x) const noexcept;
    [[nodiscard]] std::span<const float> Samples() const noexcept { return m_Samples; }
    [[nodiscard]] float DomainMinimum() const noexcept { return m_DomainMinimum; }
    [[nodiscard]] float DomainMaximum() const noexcept { return m_DomainMaximum; }

private:
    float m_DomainMinimum = 0.0f;
    float m_DomainMaximum = 1.0f;
    std::vector<float> m_Samples;
};

class CurveSnapshot {
public:
    CurveSnapshot() = default;
    explicit CurveSnapshot(CurveLUT lut) : m_Lut(std::move(lut)) {}

    [[nodiscard]] float Sample(float x) const noexcept { return m_Lut.Sample(x); }
    [[nodiscard]] const CurveLUT& Lut() const noexcept { return m_Lut; }

private:
    CurveLUT m_Lut;
};

[[nodiscard]] std::shared_ptr<const CurveSnapshot> BuildCurveSnapshot(
    const CurveModel& model,
    std::size_t sampleCount,
    float domainMinimum,
    float domainMaximum,
    CurveStatus* status = nullptr);

} // namespace Engine::Curve
