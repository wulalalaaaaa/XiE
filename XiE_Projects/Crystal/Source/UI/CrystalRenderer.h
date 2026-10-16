#pragma once

#include "Platform/IRenderSurface.h"
#include "Workspace/CrystalWorkspace.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Crystal {

class CrystalProfiler;

class CrystalRenderer {
public:
    explicit CrystalRenderer(Engine::IRenderSurface& surface) : m_Surface(&surface) {}
    ~CrystalRenderer();

    bool Initialize();
    void Render(const CrystalWorkspace& workspace);
    void SetProfiler(CrystalProfiler* profiler) noexcept { m_Profiler = profiler; }
    void InvalidateGeometryCache() noexcept { m_GeometryCacheValid = false; }

    [[nodiscard]] std::uint64_t GeometryBuildCount() const noexcept {
        return m_GeometryBuildCount;
    }
    [[nodiscard]] std::uint64_t DrawCalls() const noexcept { return m_DrawCalls; }
    [[nodiscard]] std::uint64_t Vertices() const noexcept { return m_LastVertexCount; }
    [[nodiscard]] std::uint64_t UploadedBytes() const noexcept { return m_LastUploadedBytes; }
    [[nodiscard]] std::uint64_t ObjectCacheHits() const noexcept { return m_ObjectCacheHits; }
    [[nodiscard]] std::uint64_t ObjectCacheMisses() const noexcept { return m_ObjectCacheMisses; }
    [[nodiscard]] std::uint64_t TextLayoutBuildCount() const noexcept {
        return m_TextLayoutBuildCount;
    }
    [[nodiscard]] double LastGpuDrawMilliseconds() const noexcept {
        return m_LastGpuDrawMilliseconds;
    }
    [[nodiscard]] std::uint64_t GpuTimerSamples() const noexcept { return m_GpuTimerSamples; }

private:
    struct Vertex { float x, y, r, g, b, a; };
    struct TextCell { float x, y; };
    struct WorldGeometryCacheEntry {
        std::uint64_t fingerprint = 0;
        std::vector<Vertex> vertices;
    };
    void DrawRect(float x, float y, float width, float height,
                  float r, float g, float b, float a = 1.0f);
    void DrawLine(Engine::Vec2F from, Engine::Vec2F to, float width,
                  float r, float g, float b, float a = 1.0f);
    void DrawText(float x, float y, const std::string& text, float scale,
                  float r, float g, float b, float a = 1.0f);
    void AppendVertex(Vertex vertex);
    bool TryAppendCachedWorld(std::uint64_t key, std::uint64_t fingerprint);
    void StoreWorldGeometry(
        std::uint64_t key, std::uint64_t fingerprint, std::size_t startVertex);
    void Flush(int framebufferWidth, int framebufferHeight);

    Engine::IRenderSurface* m_Surface = nullptr;
    CrystalProfiler* m_Profiler = nullptr;
    std::uint32_t m_Program = 0;
    std::uint32_t m_Vao = 0;
    std::uint32_t m_Vbo = 0;
    int m_FramebufferWidth = 1;
    int m_FramebufferHeight = 1;
    std::vector<Vertex> m_Vertices;
    std::vector<Vertex> m_WorldVertices;
    std::vector<Vertex> m_TransformedVertices;
    std::vector<Vertex> m_CachedHudVertices;
    std::vector<Vertex> m_CachedWorldVertices;
    std::unordered_map<std::uint64_t, WorldGeometryCacheEntry> m_WorldGeometryCache;
    std::unordered_map<std::string, std::vector<TextCell>> m_TextLayoutCache;
    std::size_t m_VboCapacityBytes = 0;
    bool m_GeometryCacheValid = false;
    bool m_BuildingWorldGeometry = false;
    double m_CameraCenterX = 0.0;
    double m_CameraCenterY = 0.0;
    double m_CameraZoom = 1.0;
    float m_CameraViewportWidth = 1.0f;
    float m_CameraViewportHeight = 1.0f;
    int m_CachedFramebufferWidth = 0;
    int m_CachedFramebufferHeight = 0;
    bool m_HasRenderedCamera = false;
    double m_LastRenderedCameraCenterX = 0.0;
    double m_LastRenderedCameraCenterY = 0.0;
    double m_LastRenderedCameraZoom = 1.0;
    float m_LastRenderedViewportWidth = 1.0f;
    float m_LastRenderedViewportHeight = 1.0f;
    std::uint64_t m_GeometryBuildCount = 0;
    std::uint64_t m_DrawCalls = 0;
    std::uint64_t m_LastVertexCount = 0;
    std::uint64_t m_LastUploadedBytes = 0;
    std::uint64_t m_ObjectCacheHits = 0;
    std::uint64_t m_ObjectCacheMisses = 0;
    std::uint64_t m_TextLayoutBuildCount = 0;
    std::uint32_t m_GpuTimerQuery = 0;
    bool m_GpuTimerPending = false;
    double m_LastGpuDrawMilliseconds = 0.0;
    std::uint64_t m_GpuTimerSamples = 0;
};

} // namespace Crystal
