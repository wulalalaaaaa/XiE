#include "Curve/Curve.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Engine::Curve {
namespace {

constexpr float kMinimumPointDistance = 1.0e-6f;

bool IsFinite(const CurvePoint& point) noexcept {
    return std::isfinite(point.x) && std::isfinite(point.y);
}

float Secant(const CurvePoint& a, const CurvePoint& b) noexcept {
    const float width = b.x - a.x;
    return width > kMinimumPointDistance ? (b.y - a.y) / width : 0.0f;
}

float MonotonicTangent(
    std::span<const CurvePoint> points,
    std::size_t index) noexcept {
    if (points.size() < 2) {
        return 0.0f;
    }
    if (index == 0) {
        return Secant(points[0], points[1]);
    }
    if (index + 1 == points.size()) {
        return Secant(points[index - 1], points[index]);
    }

    const float before = Secant(points[index - 1], points[index]);
    const float after = Secant(points[index], points[index + 1]);
    if (before == 0.0f || after == 0.0f || std::signbit(before) != std::signbit(after)) {
        return 0.0f;
    }

    const float h0 = points[index].x - points[index - 1].x;
    const float h1 = points[index + 1].x - points[index].x;
    const float w1 = 2.0f * h1 + h0;
    const float w2 = h1 + 2.0f * h0;
    return (w1 + w2) / (w1 / before + w2 / after);
}

} // namespace

CurveStatus CurveModel::SortAndValidate() {
    for (const CurvePoint& point : m_Points) {
        if (!IsFinite(point)) {
            return CurveStatus::NonFiniteValue;
        }
    }

    std::stable_sort(m_Points.begin(), m_Points.end(), [](const auto& a, const auto& b) {
        return a.x < b.x;
    });
    for (std::size_t i = 1; i < m_Points.size(); ++i) {
        if (m_Points[i].x - m_Points[i - 1].x < kMinimumPointDistance) {
            return CurveStatus::DuplicateX;
        }
    }
    return CurveStatus::Ok;
}

CurveStatus CurveModel::AddPoint(CurvePoint point) {
    if (!IsFinite(point)) {
        return CurveStatus::NonFiniteValue;
    }
    if (point.id == 0) {
        CurvePointId highest = 0;
        for (const CurvePoint& current : m_Points) {
            highest = std::max(highest, current.id);
        }
        point.id = highest + 1;
    }

    const auto original = m_Points;
    m_Points.push_back(point);
    const CurveStatus result = SortAndValidate();
    if (result != CurveStatus::Ok) {
        m_Points = original;
    }
    return result;
}

CurveStatus CurveModel::RemovePoint(CurvePointId id) {
    const auto it = std::find_if(m_Points.begin(), m_Points.end(), [id](const auto& point) {
        return point.id == id;
    });
    if (it == m_Points.end()) {
        return CurveStatus::PointNotFound;
    }
    m_Points.erase(it);
    return CurveStatus::Ok;
}

CurveStatus CurveModel::MovePoint(CurvePointId id, float x, float y) {
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return CurveStatus::NonFiniteValue;
    }
    const auto original = m_Points;
    const auto it = std::find_if(m_Points.begin(), m_Points.end(), [id](const auto& point) {
        return point.id == id;
    });
    if (it == m_Points.end()) {
        return CurveStatus::PointNotFound;
    }
    it->x = x;
    it->y = y;
    const CurveStatus result = SortAndValidate();
    if (result != CurveStatus::Ok) {
        m_Points = original;
    }
    return result;
}

void CurveModel::Clear() noexcept {
    m_Points.clear();
}

float CurveSampler::Sample(std::span<const CurvePoint> points, float x) noexcept {
    if (points.empty()) {
        return 0.0f;
    }
    if (points.size() == 1 || x <= points.front().x) {
        return points.front().y;
    }
    if (x >= points.back().x) {
        return points.back().y;
    }

    const auto upper = std::upper_bound(points.begin(), points.end(), x, [](float value, const CurvePoint& point) {
        return value < point.x;
    });
    const std::size_t right = static_cast<std::size_t>(upper - points.begin());
    const std::size_t left = right - 1;
    const CurvePoint& a = points[left];
    const CurvePoint& b = points[right];
    const float width = b.x - a.x;
    const float t = std::clamp((x - a.x) / width, 0.0f, 1.0f);
    if (a.interpolation == CurveInterpolation::Linear) {
        return std::lerp(a.y, b.y, t);
    }

    const float m0 = MonotonicTangent(points, left);
    const float m1 = MonotonicTangent(points, right);
    const float t2 = t * t;
    const float t3 = t2 * t;
    const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
    const float h10 = t3 - 2.0f * t2 + t;
    const float h01 = -2.0f * t3 + 3.0f * t2;
    const float h11 = t3 - t2;
    return h00 * a.y + h10 * width * m0 + h01 * b.y + h11 * width * m1;
}

CurveStatus CurveLUT::Build(
    std::span<const CurvePoint> points,
    std::size_t sampleCount,
    float domainMinimum,
    float domainMaximum) {
    if (sampleCount < 2) {
        return CurveStatus::InvalidLutSize;
    }
    if (!std::isfinite(domainMinimum) || !std::isfinite(domainMaximum) ||
        domainMaximum <= domainMinimum) {
        return CurveStatus::InvalidDomain;
    }
    for (const CurvePoint& point : points) {
        if (!IsFinite(point)) {
            return CurveStatus::NonFiniteValue;
        }
    }

    m_DomainMinimum = domainMinimum;
    m_DomainMaximum = domainMaximum;
    m_Samples.resize(sampleCount);
    const float denominator = static_cast<float>(sampleCount - 1);
    for (std::size_t i = 0; i < sampleCount; ++i) {
        const float t = static_cast<float>(i) / denominator;
        m_Samples[i] = CurveSampler::Sample(points, std::lerp(domainMinimum, domainMaximum, t));
    }
    return CurveStatus::Ok;
}

float CurveLUT::Sample(float x) const noexcept {
    if (m_Samples.empty()) {
        return 0.0f;
    }
    const float normalized = std::clamp(
        (x - m_DomainMinimum) / (m_DomainMaximum - m_DomainMinimum),
        0.0f,
        1.0f);
    const float position = normalized * static_cast<float>(m_Samples.size() - 1);
    const std::size_t left = static_cast<std::size_t>(position);
    const std::size_t right = std::min(left + 1, m_Samples.size() - 1);
    return std::lerp(m_Samples[left], m_Samples[right], position - static_cast<float>(left));
}

std::shared_ptr<const CurveSnapshot> BuildCurveSnapshot(
    const CurveModel& model,
    std::size_t sampleCount,
    float domainMinimum,
    float domainMaximum,
    CurveStatus* status) {
    CurveLUT lut;
    const CurveStatus result = lut.Build(model.Points(), sampleCount, domainMinimum, domainMaximum);
    if (status) {
        *status = result;
    }
    if (result != CurveStatus::Ok) {
        return {};
    }
    return std::make_shared<const CurveSnapshot>(std::move(lut));
}

} // namespace Engine::Curve
