#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Crystal {

// Small, dependency-free instrumentation for the native client. It is
// intentionally aggregate-only: the CSV is useful for comparing interaction
// changes without turning every frame into a trace capture.
class CrystalProfiler {
public:
    struct Sample {
        std::uint64_t count = 0;
        double totalMs = 0.0;
        double maxMs = 0.0;
    };

    struct Statistics {
        std::uint64_t framesRendered = 0;
        std::uint64_t framesSkipped = 0;
        std::uint64_t renderRequests = 0;
        std::uint64_t presentCount = 0;
        std::uint64_t drawCalls = 0;
        std::uint64_t vertices = 0;
        std::uint64_t uploadedBytes = 0;
        std::uint64_t geometryBuilds = 0;
        std::uint64_t objectCacheHits = 0;
        std::uint64_t objectCacheMisses = 0;
        std::uint64_t textLayoutBuilds = 0;
        double gpuDrawTime = 0.0;
        std::uint64_t gpuTimerSamples = 0;
        std::uint64_t workspaceViews = 0;
        std::uint64_t relationships = 0;
        std::uint64_t inkStrokes = 0;
        std::uint64_t boards = 0;
        std::uint64_t widgets = 0;
        std::string lastFrameReason = "None";
        std::unordered_map<std::string, std::uint64_t> frameReasons;
    };

    class Scope {
    public:
        Scope(CrystalProfiler* profiler, std::string_view name)
            : m_Profiler(profiler), m_Name(name), m_Start(std::chrono::steady_clock::now()) {}
        ~Scope() {
            if (m_Profiler != nullptr) {
                const auto elapsed = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - m_Start).count();
                m_Profiler->Record(m_Name, elapsed);
            }
        }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

    private:
        CrystalProfiler* m_Profiler = nullptr;
        std::string m_Name;
        std::chrono::steady_clock::time_point m_Start;
    };

    void BeginFrame() {
        ++m_Frame;
        if (m_Frame % 120 == 0) Flush();
    }

    void Record(std::string_view name, double milliseconds) {
        Sample& sample = m_Samples[std::string(name)];
        ++sample.count;
        sample.totalMs += milliseconds;
        sample.maxMs = std::max(sample.maxMs, milliseconds);
        Sample& total = m_TotalSamples[std::string(name)];
        ++total.count;
        total.totalMs += milliseconds;
        total.maxMs = std::max(total.maxMs, milliseconds);
    }

    void RecordFrameRequest(std::string_view reason) {
        ++m_Statistics.renderRequests;
        ++m_Statistics.frameReasons[std::string(reason)];
        m_Statistics.lastFrameReason = reason;
    }

    void RecordFrameRendered(std::string_view reason) {
        ++m_Statistics.framesRendered;
        m_Statistics.lastFrameReason = reason;
    }

    void RecordFrameSkipped(std::string_view reason) {
        ++m_Statistics.framesSkipped;
        m_Statistics.lastFrameReason = reason;
    }

    void RecordPresent() noexcept { ++m_Statistics.presentCount; }

    void SetRendererStats(
        std::uint64_t drawCalls, std::uint64_t vertices, std::uint64_t uploadedBytes,
        std::uint64_t geometryBuilds, std::uint64_t objectCacheHits,
        std::uint64_t objectCacheMisses, std::uint64_t textLayoutBuilds,
        double gpuDrawTime, std::uint64_t gpuTimerSamples) noexcept {
        m_Statistics.drawCalls = drawCalls;
        m_Statistics.vertices = vertices;
        m_Statistics.uploadedBytes = uploadedBytes;
        m_Statistics.geometryBuilds = geometryBuilds;
        m_Statistics.objectCacheHits = objectCacheHits;
        m_Statistics.objectCacheMisses = objectCacheMisses;
        m_Statistics.textLayoutBuilds = textLayoutBuilds;
        m_Statistics.gpuDrawTime = gpuDrawTime;
        m_Statistics.gpuTimerSamples = gpuTimerSamples;
    }

    void SetWorkspaceCounts(
        std::size_t views, std::size_t relationships, std::size_t inkStrokes,
        std::size_t boards, std::size_t widgets) noexcept {
        m_Statistics.workspaceViews = static_cast<std::uint64_t>(views);
        m_Statistics.relationships = static_cast<std::uint64_t>(relationships);
        m_Statistics.inkStrokes = static_cast<std::uint64_t>(inkStrokes);
        m_Statistics.boards = static_cast<std::uint64_t>(boards);
        m_Statistics.widgets = static_cast<std::uint64_t>(widgets);
    }

    [[nodiscard]] const Statistics& Stats() const noexcept { return m_Statistics; }

    [[nodiscard]] double AverageMilliseconds(std::string_view name) const {
        const auto found = m_TotalSamples.find(std::string(name));
        return found == m_TotalSamples.end() || found->second.count == 0
            ? 0.0 : found->second.totalMs / static_cast<double>(found->second.count);
    }

    void WriteSummary(std::string_view path = "CrystalFrameStats.csv") {
        Flush();
        std::ofstream output(std::string(path), std::ios::trunc);
        if (!output) return;
        output << "metric,value\n";
        output << "FramesRendered," << m_Statistics.framesRendered << '\n';
        output << "FramesSkipped," << m_Statistics.framesSkipped << '\n';
        output << "RenderRequests," << m_Statistics.renderRequests << '\n';
        output << "PresentCount," << m_Statistics.presentCount << '\n';
        output << "DrawCalls," << m_Statistics.drawCalls << '\n';
        output << "Vertices," << m_Statistics.vertices << '\n';
        output << "UploadedBytes," << m_Statistics.uploadedBytes << '\n';
        output << "GeometryBuilds," << m_Statistics.geometryBuilds << '\n';
        output << "ObjectCacheHits," << m_Statistics.objectCacheHits << '\n';
        output << "ObjectCacheMisses," << m_Statistics.objectCacheMisses << '\n';
        output << "TextLayoutBuilds," << m_Statistics.textLayoutBuilds << '\n';
        output << "GpuDrawTime," << m_Statistics.gpuDrawTime << '\n';
        output << "GpuTimerSamples," << m_Statistics.gpuTimerSamples << '\n';
        output << "AverageFrameTime," << AverageMilliseconds("FrameTotal") << '\n';
        output << "RenderBuildTime," << AverageMilliseconds("RenderBuild") << '\n';
        output << "PresentTime," << AverageMilliseconds("Present") << '\n';
        output << "EventPump," << AverageMilliseconds("EventPump") << '\n';
        output << "RealtimePump," << AverageMilliseconds("RealtimePump") << '\n';
        output << "Update," << AverageMilliseconds("Update") << '\n';
        output << "IdleWait," << AverageMilliseconds("IdleWait") << '\n';
        output << "WorkspaceViews," << m_Statistics.workspaceViews << '\n';
        output << "Relationships," << m_Statistics.relationships << '\n';
        output << "InkStrokes," << m_Statistics.inkStrokes << '\n';
        output << "Boards," << m_Statistics.boards << '\n';
        output << "Widgets," << m_Statistics.widgets << '\n';
        for (const auto& [reason, count] : m_Statistics.frameReasons)
            output << "FrameReason." << reason << ',' << count << '\n';
    }

    void Flush() {
        if (m_Samples.empty()) return;
        std::ofstream output("CrystalProfile.csv", std::ios::app);
        if (!output) return;
        if (!m_HeaderWritten) {
            output << "frame,category,count,avg_ms,max_ms\n";
            m_HeaderWritten = true;
        }
        for (const auto& [category, sample] : m_Samples) {
            output << m_Frame << ',' << category << ',' << sample.count << ','
                << (sample.totalMs / static_cast<double>(sample.count)) << ','
                << sample.maxMs << '\n';
        }
        m_Samples.clear();
    }

    [[nodiscard]] std::uint64_t Frame() const noexcept { return m_Frame; }

private:
    std::uint64_t m_Frame = 0;
    bool m_HeaderWritten = false;
    std::unordered_map<std::string, Sample> m_Samples;
    std::unordered_map<std::string, Sample> m_TotalSamples;
    Statistics m_Statistics;
};

} // namespace Crystal
