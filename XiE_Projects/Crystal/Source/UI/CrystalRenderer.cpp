#include "UI/CrystalRenderer.h"
#include "UI/CrystalRenderGeometry.h"
#include "UI/CrystalProfiler.h"
#include "UI/CrystalTheme.h"

#include <glad/gl.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <string_view>

namespace Crystal {
namespace {

GLuint Compile(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_FALSE) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

std::array<std::string_view, 7> Glyph(char input) {
    const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(input)));
    switch (c) {
    case 'A': return {"01110", "10001", "10001", "11111", "10001", "10001", "10001"};
    case 'B': return {"11110", "10001", "10001", "11110", "10001", "10001", "11110"};
    case 'C': return {"01111", "10000", "10000", "10000", "10000", "10000", "01111"};
    case 'D': return {"11110", "10001", "10001", "10001", "10001", "10001", "11110"};
    case 'E': return {"11111", "10000", "10000", "11110", "10000", "10000", "11111"};
    case 'F': return {"11111", "10000", "10000", "11110", "10000", "10000", "10000"};
    case 'G': return {"01111", "10000", "10000", "10111", "10001", "10001", "01111"};
    case 'H': return {"10001", "10001", "10001", "11111", "10001", "10001", "10001"};
    case 'I': return {"11111", "00100", "00100", "00100", "00100", "00100", "11111"};
    case 'J': return {"00111", "00010", "00010", "00010", "10010", "10010", "01100"};
    case 'K': return {"10001", "10010", "10100", "11000", "10100", "10010", "10001"};
    case 'L': return {"10000", "10000", "10000", "10000", "10000", "10000", "11111"};
    case 'M': return {"10001", "11011", "10101", "10101", "10001", "10001", "10001"};
    case 'N': return {"10001", "11001", "11001", "10101", "10011", "10011", "10001"};
    case 'O': return {"01110", "10001", "10001", "10001", "10001", "10001", "01110"};
    case 'P': return {"11110", "10001", "10001", "11110", "10000", "10000", "10000"};
    case 'Q': return {"01110", "10001", "10001", "10001", "10101", "10010", "01101"};
    case 'R': return {"11110", "10001", "10001", "11110", "10100", "10010", "10001"};
    case 'S': return {"01111", "10000", "10000", "01110", "00001", "00001", "11110"};
    case 'T': return {"11111", "00100", "00100", "00100", "00100", "00100", "00100"};
    case 'U': return {"10001", "10001", "10001", "10001", "10001", "10001", "01110"};
    case 'V': return {"10001", "10001", "10001", "10001", "10001", "01010", "00100"};
    case 'W': return {"10001", "10001", "10001", "10101", "10101", "11011", "10001"};
    case 'X': return {"10001", "10001", "01010", "00100", "01010", "10001", "10001"};
    case 'Y': return {"10001", "10001", "01010", "00100", "00100", "00100", "00100"};
    case 'Z': return {"11111", "00001", "00010", "00100", "01000", "10000", "11111"};
    case '0': return {"01110", "10001", "10011", "10101", "11001", "10001", "01110"};
    case '1': return {"00100", "01100", "00100", "00100", "00100", "00100", "01110"};
    case '2': return {"01110", "10001", "00001", "00010", "00100", "01000", "11111"};
    case '3': return {"11110", "00001", "00001", "01110", "00001", "00001", "11110"};
    case '4': return {"00010", "00110", "01010", "10010", "11111", "00010", "00010"};
    case '5': return {"11111", "10000", "10000", "11110", "00001", "00001", "11110"};
    case '6': return {"01110", "10000", "10000", "11110", "10001", "10001", "01110"};
    case '7': return {"11111", "00001", "00010", "00100", "01000", "01000", "01000"};
    case '8': return {"01110", "10001", "10001", "01110", "10001", "10001", "01110"};
    case '9': return {"01110", "10001", "10001", "01111", "00001", "00001", "01110"};
    case ':': return {"00000", "00100", "00100", "00000", "00100", "00100", "00000"};
    case '-': return {"00000", "00000", "00000", "11111", "00000", "00000", "00000"};
    case '.': return {"00000", "00000", "00000", "00000", "00000", "00110", "00110"};
    case '%': return {"11001", "11010", "00010", "00100", "01000", "01011", "10011"};
    case '/': return {"00001", "00010", "00100", "01000", "10000", "00000", "00000"};
    case '[': return {"01110", "01000", "01000", "01000", "01000", "01000", "01110"};
    case ']': return {"01110", "00010", "00010", "00010", "00010", "00010", "01110"};
    default: return {"00000", "00000", "00000", "00000", "00000", "00000", "00000"};
    }
}

std::string JsonText(const Engine::Json::Value& value) {
    if (value.IsString()) return value.AsString();
    if (value.IsBool()) return value.AsBool() ? "true" : "false";
    if (value.IsNumber()) {
        const double number = value.AsNumber();
        if (std::floor(number) == number) return std::to_string(static_cast<int>(number));
        std::string text = std::to_string(number);
        while (!text.empty() && text.back() == '0') text.pop_back();
        if (!text.empty() && text.back() == '.') text.pop_back();
        return text;
    }
    return value.Dump();
}

std::uint64_t HashText(std::string_view text) noexcept {
    std::uint64_t hash = 1469598103934665603ull;
    for (const unsigned char character : text) {
        hash ^= character;
        hash *= 1099511628211ull;
    }
    return hash;
}

void MixHash(std::uint64_t& hash, std::uint64_t value) noexcept {
    hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6u) + (hash >> 2u);
}

std::uint64_t HashFloat(float value) noexcept {
    return std::bit_cast<std::uint32_t>(value);
}

std::uint64_t ObjectKey(std::uint64_t type, std::string_view id) noexcept {
    std::uint64_t key = HashText(id);
    MixHash(key, type);
    return key;
}

} // namespace

CrystalRenderer::~CrystalRenderer() {
#ifndef NDEBUG
    if (m_GpuTimerQuery != 0) glDeleteQueries(1, &m_GpuTimerQuery);
#endif
    if (m_Vbo != 0) glDeleteBuffers(1, &m_Vbo);
    if (m_Vao != 0) glDeleteVertexArrays(1, &m_Vao);
    if (m_Program != 0) glDeleteProgram(m_Program);
}

bool CrystalRenderer::Initialize() {
    if (m_Surface == nullptr) return false;
    constexpr const char* vertexSource = R"(
        #version 330 core
        layout(location = 0) in vec2 aPosition;
        layout(location = 1) in vec4 aColor;
        uniform vec2 uViewport;
        out vec4 vColor;
        void main() {
            float x = aPosition.x / uViewport.x * 2.0 - 1.0;
            float y = 1.0 - aPosition.y / uViewport.y * 2.0;
            gl_Position = vec4(x, y, 0.0, 1.0);
            vColor = aColor;
        })";
    constexpr const char* fragmentSource = R"(
        #version 330 core
        in vec4 vColor;
        out vec4 color;
        void main() { color = vColor; })";
    const GLuint vertex = Compile(GL_VERTEX_SHADER, vertexSource);
    const GLuint fragment = Compile(GL_FRAGMENT_SHADER, fragmentSource);
    if (vertex == 0 || fragment == 0) return false;
    m_Program = glCreateProgram();
    glAttachShader(m_Program, vertex);
    glAttachShader(m_Program, fragment);
    glLinkProgram(m_Program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked = GL_FALSE;
    glGetProgramiv(m_Program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) return false;
    glGenVertexArrays(1, &m_Vao);
    glGenBuffers(1, &m_Vbo);
    glBindVertexArray(m_Vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_Vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(2 * sizeof(float)));
    glBindVertexArray(0);
    m_Vertices.reserve(256u * 1024u);
    m_WorldVertices.reserve(256u * 1024u);
    m_TransformedVertices.reserve(256u * 1024u);
    m_WorldGeometryCache.reserve(1024u);
    m_TextLayoutCache.reserve(512u);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#ifndef NDEBUG
    if (glGenQueries != nullptr && glBeginQuery != nullptr && glEndQuery != nullptr &&
        glGetQueryObjectiv != nullptr && glGetQueryObjectui64v != nullptr)
        glGenQueries(1, &m_GpuTimerQuery);
#endif
    return true;
}

void CrystalRenderer::DrawRect(float x, float y, float width, float height,
    float r, float g, float b, float a) {
    const Vertex p0{x, y, r, g, b, a};
    const Vertex p1{x + width, y, r, g, b, a};
    const Vertex p2{x + width, y + height, r, g, b, a};
    const Vertex p3{x, y + height, r, g, b, a};
    AppendVertex(p0);
    AppendVertex(p1);
    AppendVertex(p2);
    AppendVertex(p0);
    AppendVertex(p2);
    AppendVertex(p3);
}

void CrystalRenderer::DrawLine(Engine::Vec2F from, Engine::Vec2F to, float width,
    float r, float g, float b, float a) {
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.01f) return;
    const float nx = -dy / length * width * 0.5f;
    const float ny = dx / length * width * 0.5f;
    const Vertex p0{from.x + nx, from.y + ny, r, g, b, a};
    const Vertex p1{to.x + nx, to.y + ny, r, g, b, a};
    const Vertex p2{to.x - nx, to.y - ny, r, g, b, a};
    const Vertex p3{from.x - nx, from.y - ny, r, g, b, a};
    AppendVertex(p0);
    AppendVertex(p1);
    AppendVertex(p2);
    AppendVertex(p0);
    AppendVertex(p2);
    AppendVertex(p3);
}

void CrystalRenderer::AppendVertex(Vertex vertex) {
    if (!m_BuildingWorldGeometry) {
        m_Vertices.push_back(vertex);
        return;
    }
    const double zoom = std::max(1.0e-6, m_CameraZoom);
    const double halfWidth = static_cast<double>(m_CameraViewportWidth) * 0.5;
    const double halfHeight = static_cast<double>(m_CameraViewportHeight) * 0.5;
    vertex.x = static_cast<float>((static_cast<double>(vertex.x) - halfWidth) / zoom + m_CameraCenterX);
    vertex.y = static_cast<float>((static_cast<double>(vertex.y) - halfHeight) / zoom + m_CameraCenterY);
    m_WorldVertices.push_back(vertex);
}

bool CrystalRenderer::TryAppendCachedWorld(
    std::uint64_t key, std::uint64_t fingerprint) {
    const auto found = m_WorldGeometryCache.find(key);
    if (found == m_WorldGeometryCache.end() || found->second.fingerprint != fingerprint)
        return false;
    m_WorldVertices.insert(m_WorldVertices.end(),
        found->second.vertices.begin(), found->second.vertices.end());
    ++m_ObjectCacheHits;
    return true;
}

void CrystalRenderer::StoreWorldGeometry(
    std::uint64_t key, std::uint64_t fingerprint, std::size_t startVertex) {
    WorldGeometryCacheEntry& entry = m_WorldGeometryCache[key];
    entry.fingerprint = fingerprint;
    entry.vertices.assign(m_WorldVertices.begin() + static_cast<std::ptrdiff_t>(startVertex),
        m_WorldVertices.end());
    ++m_ObjectCacheMisses;
}

void CrystalRenderer::DrawText(float x, float y, const std::string& text, float scale,
    float r, float g, float b, float a) {
    const float pixel = m_BuildingWorldGeometry
        ? scale * static_cast<float>(std::max(1.0e-6, m_CameraZoom))
        : std::max(1.0f, scale);
    auto found = m_TextLayoutCache.find(text);
    if (found == m_TextLayoutCache.end()) {
        std::vector<TextCell> cells;
        cells.reserve(text.size() * 8u);
        float cursorUnits = 0.0f;
        for (char character : text) {
            const auto glyph = Glyph(character);
            for (std::size_t row = 0; row < glyph.size(); ++row)
                for (std::size_t column = 0; column < glyph[row].size(); ++column)
                    if (glyph[row][column] == '1')
                        cells.push_back({cursorUnits + static_cast<float>(column),
                            static_cast<float>(row)});
            cursorUnits += 6.0f;
        }
        found = m_TextLayoutCache.emplace(text, std::move(cells)).first;
        ++m_TextLayoutBuildCount;
    }
    for (const TextCell cell : found->second)
        DrawRect(x + cell.x * pixel, y + cell.y * pixel, pixel, pixel, r, g, b, a);
}

void CrystalRenderer::Flush(int framebufferWidth, int framebufferHeight) {
    m_TransformedVertices.clear();
    m_TransformedVertices.reserve(m_WorldVertices.size() + m_Vertices.size());
    const double zoom = std::max(1.0e-6, m_CameraZoom);
    const double halfWidth = static_cast<double>(m_CameraViewportWidth) * 0.5;
    const double halfHeight = static_cast<double>(m_CameraViewportHeight) * 0.5;
    for (const Vertex& source : m_WorldVertices) {
        Vertex transformed = source;
        transformed.x = static_cast<float>((static_cast<double>(source.x) - m_CameraCenterX) * zoom + halfWidth);
        transformed.y = static_cast<float>((static_cast<double>(source.y) - m_CameraCenterY) * zoom + halfHeight);
        m_TransformedVertices.push_back(transformed);
    }
    m_TransformedVertices.insert(
        m_TransformedVertices.end(), m_Vertices.begin(), m_Vertices.end());
    m_LastVertexCount = static_cast<std::uint64_t>(m_TransformedVertices.size());
    m_LastUploadedBytes = 0;
    if (m_TransformedVertices.empty() || m_Program == 0) return;
#ifndef NDEBUG
    if (m_GpuTimerQuery != 0 && m_GpuTimerPending) {
        GLint available = GL_FALSE;
        glGetQueryObjectiv(m_GpuTimerQuery, GL_QUERY_RESULT_AVAILABLE, &available);
        if (available == GL_TRUE) {
            GLuint64 elapsedNanoseconds = 0;
            glGetQueryObjectui64v(m_GpuTimerQuery, GL_QUERY_RESULT, &elapsedNanoseconds);
            m_LastGpuDrawMilliseconds = static_cast<double>(elapsedNanoseconds) / 1.0e6;
            ++m_GpuTimerSamples;
            m_GpuTimerPending = false;
        }
    }
    const bool startGpuTimer = m_GpuTimerQuery != 0 && !m_GpuTimerPending;
    if (startGpuTimer) glBeginQuery(GL_TIME_ELAPSED, m_GpuTimerQuery);
#endif
    const auto submitStart = std::chrono::steady_clock::now();
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glUseProgram(m_Program);
    glUniform2f(glGetUniformLocation(m_Program, "uViewport"),
        static_cast<float>(framebufferWidth), static_cast<float>(framebufferHeight));
    glBindVertexArray(m_Vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_Vbo);
    const std::size_t requiredBytes = m_TransformedVertices.size() * sizeof(Vertex);
    if (requiredBytes > m_VboCapacityBytes) {
        std::size_t nextCapacity = std::max<std::size_t>(4096u, m_VboCapacityBytes);
        while (nextCapacity < requiredBytes) nextCapacity *= 2u;
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(nextCapacity), nullptr, GL_DYNAMIC_DRAW);
        m_VboCapacityBytes = nextCapacity;
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(requiredBytes), m_TransformedVertices.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_TransformedVertices.size()));
    ++m_DrawCalls;
    m_LastUploadedBytes = static_cast<std::uint64_t>(requiredBytes);
    glBindVertexArray(0);
    glUseProgram(0);
#ifndef NDEBUG
    if (startGpuTimer) {
        glEndQuery(GL_TIME_ELAPSED);
        m_GpuTimerPending = true;
    }
#endif
    if (m_Profiler != nullptr) {
        m_Profiler->Record("GpuSubmit", std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - submitStart).count());
    }
    m_Vertices.clear();
    m_WorldVertices.clear();
    m_TransformedVertices.clear();
}

void CrystalRenderer::Render(const CrystalWorkspace& workspace) {
    if (m_Surface == nullptr) return;
    const Engine::RenderSurfaceSize framebuffer = m_Surface->GetFramebufferSize();
    m_FramebufferWidth = framebuffer.width;
    m_FramebufferHeight = framebuffer.height;
    m_FramebufferWidth = std::max(m_FramebufferWidth, 1);
    m_FramebufferHeight = std::max(m_FramebufferHeight, 1);
    const float width = static_cast<float>(m_FramebufferWidth);
    const float height = static_cast<float>(m_FramebufferHeight);
    const auto& currentCamera = workspace.Projects().CurrentSpace().Canvas().Camera();
    m_CameraCenterX = currentCamera.centerX;
    m_CameraCenterY = currentCamera.centerY;
    m_CameraZoom = currentCamera.zoom;
    m_CameraViewportWidth = currentCamera.viewportSize.x;
    m_CameraViewportHeight = currentCamera.viewportSize.y;
    const float hudHeight = 58.0f;
    const DisplayMode displayMode = workspace.GetDisplayMode();
    const bool drawOnly = displayMode == DisplayMode::Draw;
    const CrystalThemeTokens& theme = kCrystalTheme;
    const float inspectorWidth = width >= 1050.0f && displayMode != DisplayMode::Presentation ? 300.0f : 0.0f;
    const float worldWidth = width - inspectorWidth;
    glClearColor(theme.background[0], theme.background[1], theme.background[2], theme.background[3]);
    glClear(GL_COLOR_BUFFER_BIT);
    const bool cameraUnchanged = m_HasRenderedCamera &&
        m_LastRenderedCameraCenterX == m_CameraCenterX &&
        m_LastRenderedCameraCenterY == m_CameraCenterY &&
        m_LastRenderedCameraZoom == m_CameraZoom &&
        m_LastRenderedViewportWidth == m_CameraViewportWidth &&
        m_LastRenderedViewportHeight == m_CameraViewportHeight;
    if (m_GeometryCacheValid && cameraUnchanged &&
        m_CachedFramebufferWidth == m_FramebufferWidth &&
        m_CachedFramebufferHeight == m_FramebufferHeight) {
        m_Vertices = m_CachedHudVertices;
        m_WorldVertices = m_CachedWorldVertices;
        m_BuildingWorldGeometry = false;
        DrawRect(worldWidth - 148, height - 42, 132, 26,
            theme.surface[0], theme.surface[1], theme.surface[2], 0.94f);
        DrawText(worldWidth - 136, height - 34,
            std::to_string(static_cast<int>(std::round(currentCamera.zoom * 100.0))) + "%", 2,
            0.80f, 0.90f, 1.0f);
        Flush(m_FramebufferWidth, m_FramebufferHeight);
        return;
    }
    const bool cacheWasInvalid = !m_GeometryCacheValid;
    m_Vertices.clear();
    m_WorldVertices.clear();
    const auto& project = workspace.Projects().Workspace();
    const auto& space = workspace.Projects().CurrentSpace();
    const auto& camera = space.Canvas();
    const float zoom = static_cast<float>(camera.Camera().zoom);
    const Engine::Vec2F viewportWorldA = camera.ScreenToWorld({0.0f, hudHeight});
    const Engine::Vec2F viewportWorldB = camera.ScreenToWorld({worldWidth, height});
    const WorldViewportBounds viewport{
        std::min(viewportWorldA.x, viewportWorldB.x),
        std::min(viewportWorldA.y, viewportWorldB.y),
        std::max(viewportWorldA.x, viewportWorldB.x),
        std::max(viewportWorldA.y, viewportWorldB.y)};
    const auto rectVisible = [&](Engine::Vec2F position, Engine::Vec2F size) {
        return WorldRectIntersectsViewport(position, size, viewport);
    };
    const auto segmentBoundsVisible = [&](Engine::Vec2F a, Engine::Vec2F b) {
        return WorldSegmentBoundsIntersectViewport(a, b, viewport);
    };
    const auto layerVisible = [&](const std::string& layerId) {
        for (const auto& layer : project.userLayers) {
            if (layer.name == layerId && (layer.spaceId.empty() || layer.spaceId == space.Current().id))
                return layer.visible;
        }
        return true;
    };
    const auto findEntity = [&](const std::string& id) -> const CrystalEntity* {
        for (const auto& entity : project.entities)
            if (entity.id == id) return &entity;
        return nullptr;
    };
    const auto findView = [&](const std::string& entityId) -> const EntityView* {
        for (const auto& view : project.views)
            if (view.entityId == entityId && view.spaceId == space.Current().id && view.visible && layerVisible(view.layerId)) {
                if (!view.boardId.empty()) {
                    const auto board = std::find_if(project.boards.begin(), project.boards.end(),
                        [&](const BoardModel& item) { return item.id == view.boardId; });
                    if (board != project.boards.end() && board->collapsed) continue;
                }
                return &view;
            }
        return nullptr;
    };
    const auto ownerName = [&](const std::string& workerId) {
        const CrystalEntity* owner = findEntity(workerId);
        return owner == nullptr ? workerId : owner->title;
    };

    // System/HUD layer: all coordinates remain screen-fixed.
    DrawRect(0, 0, width, hudHeight, theme.surface[0], theme.surface[1], theme.surface[2], theme.surface[3]);
    DrawText(18, 15, "CRYSTAL", 3, theme.foreground[0], theme.foreground[1], theme.foreground[2], 1.0f);
    DrawText(155, 13, project.project.name.empty() ? "OFFLINE" : project.project.name, 2,
        theme.foreground[0], theme.foreground[1], theme.foreground[2]);
    DrawText(155, 31, space.Current().name.empty() ? "Overview" : space.Current().name, 2,
        theme.muted[0], theme.muted[1], theme.muted[2]);
    DrawText(worldWidth - 300, 31, DisplayModeName(displayMode), 1.25f,
        theme.accent[0], theme.accent[1], theme.accent[2], 1.0f);
    float recentX = 155.0f;
    for (const auto& candidate : project.spaces) {
        if (candidate.id == space.Current().id || recentX > 330.0f) continue;
        DrawText(recentX, 48, candidate.name, 0.85f,
            theme.disabled[0], theme.disabled[1], theme.disabled[2]);
        recentX += std::max(42.0f, static_cast<float>(candidate.name.size()) * 5.5f + 12.0f);
    }
    if (displayMode != DisplayMode::Presentation) {
        DrawRect(245, 13, 75, 30, theme.elevated[0], theme.elevated[1], theme.elevated[2], theme.elevated[3]);
        DrawText(253, 22, "SEARCH", 1.5f, theme.accent[0], theme.accent[1], theme.accent[2], 1.0f);
        const float toolsX = 335.0f;
        const std::array<std::pair<const char*, Engine::UI2D::CanvasTool>, 5> tools = {{
            {"SELECT", Engine::UI2D::CanvasTool::Select},
            {"NODE", Engine::UI2D::CanvasTool::Node},
            {"CONNECT", Engine::UI2D::CanvasTool::Connect},
            {"PEN", Engine::UI2D::CanvasTool::Pen},
            {"ERASER", Engine::UI2D::CanvasTool::Eraser}}};
        for (std::size_t i = 0; i < tools.size(); ++i) {
            const bool selected = workspace.Tools().Tool() == tools[i].second;
            DrawRect(toolsX + static_cast<float>(i) * 90.0f, 9, 82, 38,
                selected ? theme.hover[0] * 0.60f : theme.surface[0],
                selected ? theme.hover[1] * 0.60f : theme.surface[1],
                selected ? theme.hover[2] * 0.60f : theme.surface[2]);
            DrawText(toolsX + 8 + static_cast<float>(i) * 90.0f, 22, tools[i].first, 2,
                theme.foreground[0], theme.foreground[1], theme.foreground[2]);
        }
    }
    const std::string connectionStatus = workspace.ConnectionStatus();
    const bool connected = connectionStatus == "Connected";
    const bool reconnecting = connectionStatus == "Reconnecting" || connectionStatus == "Connecting";
    const float* connectionColor = connected ? theme.success : reconnecting ? theme.warning : theme.offline;
    DrawRect(worldWidth - 182, 17, 10, 10,
        connectionColor[0], connectionColor[1], connectionColor[2]);
    DrawText(worldWidth - 160, 18, connectionStatus, 1.7f,
        connectionColor[0], connectionColor[1], connectionColor[2]);

    if (displayMode != DisplayMode::Presentation && !workspace.SearchQuery().empty()) {
        const float searchHeight = std::min(220.0f,
            34.0f + static_cast<float>(workspace.SearchResults().size()) * 28.0f);
        DrawRect(245, hudHeight, 300, searchHeight,
            theme.elevated[0], theme.elevated[1], theme.elevated[2], 0.98f);
        DrawText(255, hudHeight + 12, workspace.SearchQuery(), 1.5f,
            theme.foreground[0], theme.foreground[1], theme.foreground[2]);
        for (std::size_t index = 0; index < workspace.SearchResults().size() && index < 6; ++index) {
            const auto& result = workspace.SearchResults()[index];
            DrawText(255, hudHeight + 38 + static_cast<float>(index) * 28.0f,
                result.title, 1.3f, 0.72f, 0.84f, 0.94f);
        }
        DrawText(255, hudHeight + searchHeight - 10, "V ADD VIEW", 1.0f, 0.54f, 0.74f, 0.88f);
    }

    const bool taskBoard = space.Current().viewType == SpaceViewType::TaskBoard;
    if (taskBoard && !drawOnly) {
        DrawRect(0, hudHeight, worldWidth, height - hudHeight, 0.055f, 0.07f, 0.10f);
        constexpr std::array<const char*, 7> columns = {
            "BACKLOG", "READY", "INPROGRESS", "REVIEW", "TESTING", "DONE", "BLOCKED"};
        const float columnWidth = worldWidth / static_cast<float>(columns.size());
        for (std::size_t column = 0; column < columns.size(); ++column) {
            const float x = static_cast<float>(column) * columnWidth;
            DrawRect(x + 5, hudHeight + 8, columnWidth - 10, 28, 0.10f, 0.15f, 0.22f);
            DrawText(x + 12, hudHeight + 18, columns[column], 1.5f, 0.82f, 0.92f, 1.0f);
        }
        std::array<int, columns.size()> cardCounts{};
        for (const auto& entity : project.entities) {
            if (entity.type != EntityType::Task) continue;
            const auto* task = std::get_if<TaskDetails>(&entity.details);
            const std::string status = task == nullptr ? "Backlog" : TaskStatusName(task->status);
            std::size_t column = 0;
            if (status == "Ready") column = 1;
            else if (status == "InProgress") column = 2;
            else if (status == "Review") column = 3;
            else if (status == "Testing") column = 4;
            else if (status == "Done") column = 5;
            else if (status == "Blocked") column = 6;
            const float cardY = hudHeight + 48 + static_cast<float>(cardCounts[column]++) * 92.0f;
            const float cardX = static_cast<float>(column) * columnWidth + 12;
            DrawRect(cardX, cardY, columnWidth - 24, 80, 0.12f, 0.22f, 0.32f, 0.96f);
            DrawText(cardX + 8, cardY + 12, entity.title, 1.5f, 0.95f, 0.98f, 1.0f);
            DrawText(cardX + 8, cardY + 34, status, 1.3f, 0.60f, 0.88f, 1.0f);
            if (task != nullptr) {
                DrawText(cardX + 8, cardY + 54, ownerName(task->assignedWorkerId), 1.1f, 0.90f, 0.82f, 0.70f);
                DrawText(cardX + columnWidth - 70, cardY + 54, TaskPriorityName(task->priority), 1.1f,
                    1.0f, 0.72f, 0.48f);
            }
        }
    } else {
        // Graph and Ink use the same world-to-screen transform.
        m_BuildingWorldGeometry = true;
        const Engine::Vec2F topLeft = camera.ScreenToWorld({0, hudHeight});
        const Engine::Vec2F bottomRight = camera.ScreenToWorld({worldWidth, height});
        const float gridStep = 100.0f;
        const int firstX = static_cast<int>(std::floor(topLeft.x / gridStep)) - 1;
        const int lastX = static_cast<int>(std::ceil(bottomRight.x / gridStep)) + 1;
        const int firstY = static_cast<int>(std::floor(topLeft.y / gridStep)) - 1;
        const int lastY = static_cast<int>(std::ceil(bottomRight.y / gridStep)) + 1;
        for (int gridX = firstX; gridX <= lastX && gridX < firstX + 120 && !drawOnly; ++gridX) {
            const float x = camera.WorldToScreen({gridX * gridStep, 0}).x;
            DrawLine({x, hudHeight}, {x, height}, 1, 0.09f, 0.11f, 0.15f);
        }
        for (int gridY = firstY; gridY <= lastY && gridY < firstY + 120 && !drawOnly; ++gridY) {
            const float y = camera.WorldToScreen({0, gridY * gridStep}).y;
            DrawLine({0, y}, {worldWidth, y}, 1, 0.09f, 0.11f, 0.15f);
        }
        for (const auto& board : project.boards) {
            if (board.spaceId != space.Current().id || !layerVisible(board.layerId)) continue;
            if (!rectVisible(board.position,
                    {board.size.x, board.collapsed ? 42.0f : board.size.y})) continue;
            const std::uint64_t boardKey = ObjectKey(1u, board.id);
            std::uint64_t boardFingerprint = HashText(board.id);
            MixHash(boardFingerprint, HashText(board.projectId));
            MixHash(boardFingerprint, HashText(board.spaceId));
            MixHash(boardFingerprint, HashText(board.title));
            MixHash(boardFingerprint, HashText(board.background));
            MixHash(boardFingerprint, HashText(board.border));
            MixHash(boardFingerprint, HashFloat(board.position.x));
            MixHash(boardFingerprint, HashFloat(board.position.y));
            MixHash(boardFingerprint, HashFloat(board.size.x));
            MixHash(boardFingerprint, HashFloat(board.size.y));
            MixHash(boardFingerprint, HashText(board.layerId));
            MixHash(boardFingerprint, static_cast<std::uint64_t>(board.locked));
            MixHash(boardFingerprint, static_cast<std::uint64_t>(board.collapsed));
            MixHash(boardFingerprint, static_cast<std::uint64_t>(board.zOrder));
            const bool selectedBoard = workspace.Selection().IsSelected(
                Engine::UI2D::SelectionKind::Board, board.id);
            const bool hoverBoard = workspace.BoardHoverId() == board.id;
            const bool hoverLocked = hoverBoard && workspace.BoardHoverLocked();
            MixHash(boardFingerprint, static_cast<std::uint64_t>(selectedBoard));
            MixHash(boardFingerprint, static_cast<std::uint64_t>(hoverBoard));
            MixHash(boardFingerprint, static_cast<std::uint64_t>(hoverLocked));
            MixHash(boardFingerprint, static_cast<std::uint64_t>(drawOnly));
            if (TryAppendCachedWorld(boardKey, boardFingerprint)) continue;
            const std::size_t boardStartVertex = m_WorldVertices.size();
            const Engine::Vec2F boardScreen = camera.WorldToScreen(board.position);
            const float boardWidth = board.size.x * zoom;
            const float boardHeight = (board.collapsed ? 42.0f : board.size.y) * zoom;
            DrawRect(boardScreen.x, boardScreen.y, boardWidth, boardHeight,
                hoverLocked ? theme.locked[0] * 0.45f : hoverBoard ? theme.hover[0] * 0.35f :
                    board.locked ? 0.10f : selectedBoard ? 0.12f : 0.08f,
                hoverLocked ? theme.locked[1] * 0.45f : hoverBoard ? theme.hover[1] * 0.35f :
                    selectedBoard ? 0.22f : 0.14f,
                hoverLocked ? theme.locked[2] * 0.45f : hoverBoard ? theme.hover[2] * 0.35f :
                    selectedBoard ? 0.34f : 0.22f,
                drawOnly ? 0.55f : 0.90f);
            DrawRect(boardScreen.x, boardScreen.y, boardWidth, 4.0f,
                hoverLocked ? theme.locked[0] : hoverBoard ? theme.hover[0] :
                    selectedBoard ? theme.selection[0] : board.locked ? 0.80f : 0.35f,
                hoverLocked ? theme.locked[1] : hoverBoard ? theme.hover[1] :
                    selectedBoard ? theme.selection[1] : 0.55f,
                hoverLocked ? theme.locked[2] : hoverBoard ? theme.hover[2] :
                    selectedBoard ? theme.selection[2] : 0.78f, 0.95f);
            std::string boardTitle = board.title;
            if (workspace.InlineEdit().active && workspace.InlineEdit().targetType == "Board" &&
                workspace.InlineEdit().targetId == board.id)
                boardTitle = workspace.InlineEdit().draft;
            DrawText(boardScreen.x + 12, boardScreen.y + 14, "BOARD " + boardTitle,
                1.45f, 0.72f, 0.88f, 1.0f);
            if (hoverBoard)
                DrawText(boardScreen.x + boardWidth - 128, boardScreen.y + 14,
                    hoverLocked ? "LOCKED" : "RELEASE TO ATTACH", 0.82f,
                    hoverLocked ? theme.locked[0] : theme.hover[0],
                    hoverLocked ? theme.locked[1] : theme.hover[1],
                    hoverLocked ? theme.locked[2] : theme.hover[2], 1.0f);
            DrawText(boardScreen.x + boardWidth - 78, boardScreen.y + 14,
                board.collapsed ? "COLLAPSED" : board.locked ? "LOCKED" : "FREE", 1.0f,
                board.locked ? theme.locked[0] : theme.muted[0],
                board.locked ? theme.locked[1] : theme.muted[1],
                board.locked ? theme.locked[2] : theme.muted[2]);
            if (!board.collapsed)
                DrawLine({boardScreen.x, boardScreen.y + boardHeight},
                         {boardScreen.x + boardWidth, boardScreen.y + boardHeight}, 2,
                         0.25f, 0.43f, 0.62f, 0.85f);
            StoreWorldGeometry(boardKey, boardFingerprint, boardStartVertex);
        }
        const auto boardCollapsed = [&](const EntityView& view) {
            if (view.boardId.empty()) return false;
            const auto board = std::find_if(project.boards.begin(), project.boards.end(),
                [&](const BoardModel& item) { return item.id == view.boardId; });
            return board != project.boards.end() && board->collapsed;
        };
        for (const auto& stroke : workspace.Ink().Strokes()) {
            if (displayMode == DisplayMode::Graph) continue;
            if (!layerVisible(stroke.layer)) continue;
            Engine::Vec2F anchorDelta{};
            if (!stroke.anchorEntityId.empty()) {
                if (const EntityView* anchor = findView(stroke.anchorEntityId)) {
                    anchorDelta = {anchor->position.x - stroke.localOffset.x,
                                   anchor->position.y - stroke.localOffset.y};
                }
            } else if (!stroke.anchorBoardId.empty()) {
                const auto board = std::find_if(project.boards.begin(), project.boards.end(),
                    [&](const BoardModel& item) { return item.id == stroke.anchorBoardId; });
                if (board != project.boards.end()) {
                    anchorDelta = {board->position.x - stroke.localOffset.x,
                                   board->position.y - stroke.localOffset.y};
                }
            }
            if (stroke.points.size() < 2) continue;
            Engine::Vec2F strokeMin{stroke.points.front().x + anchorDelta.x,
                                    stroke.points.front().y + anchorDelta.y};
            Engine::Vec2F strokeMax = strokeMin;
            for (const Engine::Vec2F point : stroke.points) {
                const Engine::Vec2F worldPoint{point.x + anchorDelta.x, point.y + anchorDelta.y};
                strokeMin.x = std::min(strokeMin.x, worldPoint.x);
                strokeMin.y = std::min(strokeMin.y, worldPoint.y);
                strokeMax.x = std::max(strokeMax.x, worldPoint.x);
                strokeMax.y = std::max(strokeMax.y, worldPoint.y);
            }
            if (!rectVisible(strokeMin, {strokeMax.x - strokeMin.x, strokeMax.y - strokeMin.y}))
                continue;
            const std::uint64_t strokeKey = ObjectKey(4u, stroke.id);
            std::uint64_t strokeFingerprint = HashText(stroke.id);
            MixHash(strokeFingerprint, HashText(stroke.layer));
            MixHash(strokeFingerprint, HashText(stroke.anchorEntityId));
            MixHash(strokeFingerprint, HashText(stroke.anchorBoardId));
            MixHash(strokeFingerprint, HashText(stroke.groupId));
            MixHash(strokeFingerprint, HashFloat(stroke.width));
            MixHash(strokeFingerprint, HashFloat(stroke.localOffset.x));
            MixHash(strokeFingerprint, HashFloat(stroke.localOffset.y));
            MixHash(strokeFingerprint, static_cast<std::uint64_t>(stroke.tool));
            MixHash(strokeFingerprint, static_cast<std::uint64_t>(stroke.shape));
            for (const Engine::Vec2F point : stroke.points) {
                MixHash(strokeFingerprint, HashFloat(point.x));
                MixHash(strokeFingerprint, HashFloat(point.y));
            }
            const bool selectedInk = workspace.Selection().IsInkSelected(stroke.id) ||
                (!stroke.groupId.empty() && workspace.Selection().IsSelected(
                    Engine::UI2D::SelectionKind::InkGroup, stroke.groupId));
            MixHash(strokeFingerprint, static_cast<std::uint64_t>(selectedInk));
            if (TryAppendCachedWorld(strokeKey, strokeFingerprint)) continue;
            const std::size_t strokeStartVertex = m_WorldVertices.size();
            const float lineWidth = std::max(1.0f, stroke.width * zoom);
            const float red = selectedInk ? 1.0f : stroke.tool == Engine::UI2D::StrokeTool::Highlighter ? 0.98f : 0.62f;
            const float green = selectedInk ? 0.76f : stroke.tool == Engine::UI2D::StrokeTool::Highlighter ? 0.82f : 0.82f;
            const float alpha = selectedInk ? 1.0f : stroke.tool == Engine::UI2D::StrokeTool::Highlighter ? 0.35f : 0.92f;
            const auto pointAt = [&](std::size_t index) {
                return camera.WorldToScreen({stroke.points[index].x + anchorDelta.x,
                                             stroke.points[index].y + anchorDelta.y});
            };
            if (stroke.shape == Engine::UI2D::StrokeShape::Freehand) {
                for (std::size_t index = 1; index < stroke.points.size(); ++index)
                    DrawLine(pointAt(index - 1), pointAt(index), lineWidth, red, green, 0.98f, alpha);
            } else {
                const Engine::Vec2F first = pointAt(0);
                const Engine::Vec2F last = pointAt(stroke.points.size() - 1);
                if (stroke.shape == Engine::UI2D::StrokeShape::StraightLine ||
                    stroke.shape == Engine::UI2D::StrokeShape::Arrow) {
                    DrawLine(first, last, lineWidth, red, green, 0.98f, alpha);
                    if (stroke.shape == Engine::UI2D::StrokeShape::Arrow) {
                        const float dx = last.x - first.x;
                        const float dy = last.y - first.y;
                        const float length = std::sqrt(dx * dx + dy * dy);
                        if (length > 1.0f) {
                            const float ux = dx / length;
                            const float uy = dy / length;
                            const Engine::Vec2F tip{last.x - ux * 12.0f, last.y - uy * 12.0f};
                            DrawLine(last, {tip.x - uy * 6.0f, tip.y + ux * 6.0f}, lineWidth, red, green, 0.98f, alpha);
                            DrawLine(last, {tip.x + uy * 6.0f, tip.y - ux * 6.0f}, lineWidth, red, green, 0.98f, alpha);
                        }
                    }
                } else if (stroke.shape == Engine::UI2D::StrokeShape::Rectangle) {
                    const Engine::Vec2F topLeft{std::min(first.x, last.x), std::min(first.y, last.y)};
                    const Engine::Vec2F bottomRight{std::max(first.x, last.x), std::max(first.y, last.y)};
                    const Engine::Vec2F topRight{bottomRight.x, topLeft.y};
                    const Engine::Vec2F bottomLeft{topLeft.x, bottomRight.y};
                    DrawLine(topLeft, topRight, lineWidth, red, green, 0.98f, alpha);
                    DrawLine(topRight, bottomRight, lineWidth, red, green, 0.98f, alpha);
                    DrawLine(bottomRight, bottomLeft, lineWidth, red, green, 0.98f, alpha);
                    DrawLine(bottomLeft, topLeft, lineWidth, red, green, 0.98f, alpha);
                } else {
                    const Engine::Vec2F center{(first.x + last.x) * 0.5f, (first.y + last.y) * 0.5f};
                    const float radiusX = std::abs(last.x - first.x) * 0.5f;
                    const float radiusY = std::abs(last.y - first.y) * 0.5f;
                    constexpr int segments = 24;
                    for (int segment = 1; segment <= segments; ++segment) {
                        const float previousAngle = 6.2831853f * static_cast<float>(segment - 1) / segments;
                        const float angle = 6.2831853f * static_cast<float>(segment) / segments;
                        DrawLine({center.x + std::cos(previousAngle) * radiusX,
                                  center.y + std::sin(previousAngle) * radiusY},
                                 {center.x + std::cos(angle) * radiusX,
                                  center.y + std::sin(angle) * radiusY},
                                 lineWidth, red, green, 0.98f, alpha);
                    }
                }
            }
            StoreWorldGeometry(strokeKey, strokeFingerprint, strokeStartVertex);
        }

        for (const auto& relationship : project.relationships) {
            if (drawOnly || !relationship.visible) continue;
            const EntityView* source = findView(relationship.sourceEntityId);
            const EntityView* target = findView(relationship.targetEntityId);
            if (source == nullptr || target == nullptr) continue;
            const Engine::Vec2F fromWorld{
                source->position.x + source->size.x * 0.5f, source->position.y + source->size.y * 0.5f};
            const Engine::Vec2F toWorld{
                target->position.x + target->size.x * 0.5f, target->position.y + target->size.y * 0.5f};
            if (!segmentBoundsVisible(fromWorld, toWorld)) continue;
            const bool selected = workspace.SelectedRelationshipId() == relationship.id;
            const std::uint64_t relationshipKey = ObjectKey(3u, relationship.id);
            std::uint64_t relationshipFingerprint = HashText(relationship.id);
            MixHash(relationshipFingerprint, HashText(relationship.sourceEntityId));
            MixHash(relationshipFingerprint, HashText(relationship.targetEntityId));
            MixHash(relationshipFingerprint, static_cast<std::uint64_t>(relationship.type));
            MixHash(relationshipFingerprint, static_cast<std::uint64_t>(relationship.visible));
            MixHash(relationshipFingerprint, HashFloat(fromWorld.x));
            MixHash(relationshipFingerprint, HashFloat(fromWorld.y));
            MixHash(relationshipFingerprint, HashFloat(toWorld.x));
            MixHash(relationshipFingerprint, HashFloat(toWorld.y));
            MixHash(relationshipFingerprint, static_cast<std::uint64_t>(selected));
            if (TryAppendCachedWorld(relationshipKey, relationshipFingerprint)) continue;
            const std::size_t relationshipStartVertex = m_WorldVertices.size();
            const Engine::Vec2F from = camera.WorldToScreen(fromWorld);
            const Engine::Vec2F to = camera.WorldToScreen(toWorld);
            const float r = selected ? 1.0f : relationship.type == RelationshipType::Blocks ? 0.95f : 0.34f;
            const float g = selected ? 0.86f : relationship.type == RelationshipType::DependsOn ? 0.80f : 0.62f;
            DrawLine(from, to, selected ? 4.0f : 2.0f, r, g, 0.94f, selected ? 1.0f : 0.8f);
            const float dx = to.x - from.x;
            const float dy = to.y - from.y;
            const float length = std::sqrt(dx * dx + dy * dy);
            if (length > 1.0f) {
                const float ux = dx / length;
                const float uy = dy / length;
                const Engine::Vec2F tip{to.x - ux * 8.0f, to.y - uy * 8.0f};
                DrawLine(to, {tip.x - uy * 5.0f + ux * 8.0f, tip.y + ux * 5.0f + uy * 8.0f}, 2, r, g, 0.94f);
                DrawLine(to, {tip.x + uy * 5.0f + ux * 8.0f, tip.y - ux * 5.0f + uy * 8.0f}, 2, r, g, 0.94f);
            }
            const Engine::Vec2F label{(from.x + to.x) * 0.5f, (from.y + to.y) * 0.5f};
            DrawText(label.x, label.y, RelationshipTypeName(relationship.type), 1.0f, 0.70f, 0.82f, 0.92f, 0.88f);
            StoreWorldGeometry(relationshipKey, relationshipFingerprint, relationshipStartVertex);
        }

        for (const auto& view : project.views) {
            if (view.spaceId != space.Current().id || !view.visible || !layerVisible(view.layerId) ||
                boardCollapsed(view)) continue;
            if (!rectVisible(view.position, view.size)) continue;
            const CrystalEntity* entity = findEntity(view.entityId);
            if (entity == nullptr) continue;
            const bool selected = workspace.Selection().IsSelected(
                Engine::UI2D::SelectionKind::EntityView, entity->id);
            const SceneLockModel* lock = nullptr;
            for (const auto& candidate : project.sceneLocks)
                if (candidate.sceneId == entity->id) { lock = &candidate; break; }
            const bool locked = lock != nullptr && lock->status != "FREE";
            const std::uint64_t viewKey = ObjectKey(2u, view.id);
            std::uint64_t viewFingerprint = HashText(view.id);
            MixHash(viewFingerprint, HashText(view.entityId));
            MixHash(viewFingerprint, HashText(entity->id));
            MixHash(viewFingerprint, HashText(entity->title));
            MixHash(viewFingerprint, HashText(entity->description));
            MixHash(viewFingerprint, static_cast<std::uint64_t>(entity->revision));
            MixHash(viewFingerprint, static_cast<std::uint64_t>(entity->type));
            MixHash(viewFingerprint, HashText(entity->typeName));
            MixHash(viewFingerprint, HashText(entity->schemaId));
            MixHash(viewFingerprint, static_cast<std::uint64_t>(view.revision));
            MixHash(viewFingerprint, HashFloat(view.position.x));
            MixHash(viewFingerprint, HashFloat(view.position.y));
            MixHash(viewFingerprint, HashFloat(view.size.x));
            MixHash(viewFingerprint, HashFloat(view.size.y));
            MixHash(viewFingerprint, HashText(view.layerId));
            MixHash(viewFingerprint, static_cast<std::uint64_t>(view.viewType));
            MixHash(viewFingerprint, static_cast<std::uint64_t>(selected));
            MixHash(viewFingerprint, static_cast<std::uint64_t>(locked));
            MixHash(viewFingerprint, lock == nullptr ? 0u : HashText(lock->status));
            MixHash(viewFingerprint, static_cast<std::uint64_t>(drawOnly));
            if (TryAppendCachedWorld(viewKey, viewFingerprint)) continue;
            const std::size_t viewStartVertex = m_WorldVertices.size();
            const Engine::Vec2F screen = camera.WorldToScreen(view.position);
            const float nodeWidth = view.size.x * zoom;
            const float nodeHeight = view.size.y * zoom;
            const float red = entity->type == EntityType::Scene ? (locked ? 0.55f : 0.18f)
                : entity->type == EntityType::Task ? 0.16f
                : entity->type == EntityType::System ? 0.22f : 0.12f;
            const float green = entity->type == EntityType::Scene ? (locked ? 0.20f : 0.52f)
                : entity->type == EntityType::System ? 0.40f : 0.25f;
            DrawRect(screen.x, screen.y, nodeWidth, nodeHeight, red, green, 0.34f, 0.96f);
            DrawRect(screen.x, screen.y, nodeWidth, selected ? 7.0f : 4.0f,
                selected ? 1.0f : 0.72f, selected ? 0.72f : 0.88f, 1.0f);
            DrawText(screen.x + 12, screen.y + 16, entity->title,
                2.0f, 0.95f, 0.98f, 1.0f);
            if (entity->type == EntityType::Task) {
                if (const auto* task = std::get_if<TaskDetails>(&entity->details)) {
                    DrawText(screen.x + 12, screen.y + 48, TaskStatusName(task->status), 1.5f, 0.60f, 0.88f, 1.0f);
                    DrawText(screen.x + 12, screen.y + 70, ownerName(task->assignedWorkerId), 1.3f, 0.90f, 0.82f, 0.70f);
                    DrawText(screen.x + 12, screen.y + 92, TaskPriorityName(task->priority), 1.3f, 1.0f, 0.72f, 0.48f);
                }
            } else if (entity->type == EntityType::System) {
                if (const auto* system = std::get_if<SystemDetails>(&entity->details)) {
                    DrawText(screen.x + 12, screen.y + 48, SystemStatusName(system->status), 1.5f, 0.76f, 0.90f, 0.98f);
                    DrawText(screen.x + 12, screen.y + 72, std::to_string(static_cast<int>(system->progress * 100.0)) + "%", 1.4f, 0.62f, 0.92f, 0.72f);
                }
            } else if (entity->type == EntityType::Worker) {
                if (const auto* worker = std::get_if<WorkerDetails>(&entity->details)) {
                    DrawText(screen.x + 12, screen.y + 48, WorkerTypeName(worker->type), 1.3f, 0.88f, 0.84f, 1.0f);
                    DrawText(screen.x + 12, screen.y + 68, WorkerStatusName(worker->status), 1.3f, 0.60f, 0.92f, 0.74f);
                    DrawText(screen.x + 12, screen.y + 88, worker->currentTask.empty() ? "NO TASK" : worker->currentTask, 1.1f, 0.90f, 0.82f, 0.70f);
                    DrawText(screen.x + 12, screen.y + 108, worker->branch.empty() ? "NO BRANCH" : worker->branch, 1.0f, 0.74f, 0.82f, 0.92f);
                }
            } else if (entity->type == EntityType::Milestone) {
                if (const auto* milestone = std::get_if<MilestoneDetails>(&entity->details)) {
                    DrawText(screen.x + 12, screen.y + 48, MilestoneStatusName(milestone->status), 1.4f, 0.82f, 0.88f, 1.0f);
                    DrawText(screen.x + 12, screen.y + 70, std::to_string(static_cast<int>(milestone->progress * 100.0)) + "%", 1.4f, 0.62f, 0.92f, 0.72f);
                }
            } else if (entity->type == EntityType::Scene) {
                const bool stale = lock != nullptr && lock->status == "STALE";
                DrawText(screen.x + 12, screen.y + 45, lock == nullptr ? "FREE" : lock->status, 2,
                    stale ? theme.stale[0] : locked ? theme.locked[0] : theme.muted[0],
                    stale ? theme.stale[1] : locked ? theme.locked[1] : theme.muted[1],
                    stale ? theme.stale[2] : locked ? theme.locked[2] : theme.muted[2]);
                if (lock != nullptr && locked) {
                    DrawText(screen.x + 12, screen.y + 70, lock->ownerName, 1.5f,
                        theme.warning[0], theme.warning[1], theme.warning[2]);
                    std::string lockTask = lock->taskId.empty() ? lock->task : lock->taskId;
                    if (const CrystalEntity* task = findEntity(lock->taskId); task != nullptr) lockTask = task->title;
                    DrawText(screen.x + 12, screen.y + 92, lockTask, 1.1f, 0.80f, 0.84f, 0.90f);
                }
            } else if (entity->type == EntityType::Unknown) {
                DrawText(screen.x + 12, screen.y + 46,
                    entity->typeName.empty() ? "CUSTOM" : entity->typeName,
                    1.35f, 0.72f, 0.86f, 1.0f);
                const CrystalEntitySchema* schema = nullptr;
                for (const auto& candidate : project.schemas)
                    if (candidate.id == entity->schemaId) { schema = &candidate; break; }
                std::vector<std::string> propertyNames = view.visibleProperties;
                if (propertyNames.empty() && schema != nullptr) {
                    for (const auto& definition : schema->properties) {
                        propertyNames.push_back(definition.name);
                        if (propertyNames.size() >= 3) break;
                    }
                }
                float propertyY = screen.y + 70;
                for (const std::string& propertyName : propertyNames) {
                    const Engine::Json::Value* property = entity->properties.Find(propertyName);
                    if (property == nullptr) continue;
                    DrawText(screen.x + 12, propertyY,
                        propertyName + ": " + JsonText(*property), 1.0f,
                        0.78f, 0.88f, 0.94f);
                    propertyY += 18;
                    if (propertyY > screen.y + nodeHeight - 12) break;
                }
            } else {
                DrawText(screen.x + 12, screen.y + 48, "NOTE", 1.5f, 0.58f, 0.76f, 0.92f);
            }
            StoreWorldGeometry(viewKey, viewFingerprint, viewStartVertex);
        }
    }

    m_BuildingWorldGeometry = true;
    for (const auto& widget : project.dashboardWidgets) {
        if (!widget.visible || !layerVisible(widget.layerId) || widget.spaceId != space.Current().id) continue;
        if (!rectVisible(widget.position, widget.size)) continue;
        const bool selectedWidget = workspace.Selection().IsSelected(
            Engine::UI2D::SelectionKind::DashboardWidget, widget.id);
        const std::uint64_t widgetKey = ObjectKey(5u, widget.id);
        std::uint64_t widgetFingerprint = HashText(widget.id);
        MixHash(widgetFingerprint, HashText(widget.projectId));
        MixHash(widgetFingerprint, HashText(widget.spaceId));
        MixHash(widgetFingerprint, HashText(widget.widgetType));
        MixHash(widgetFingerprint, HashText(widget.title));
        MixHash(widgetFingerprint, HashText(widget.layerId));
        MixHash(widgetFingerprint, HashFloat(widget.position.x));
        MixHash(widgetFingerprint, HashFloat(widget.position.y));
        MixHash(widgetFingerprint, HashFloat(widget.size.x));
        MixHash(widgetFingerprint, HashFloat(widget.size.y));
        MixHash(widgetFingerprint, static_cast<std::uint64_t>(widget.revision));
        MixHash(widgetFingerprint, static_cast<std::uint64_t>(selectedWidget));
        MixHash(widgetFingerprint, static_cast<std::uint64_t>(drawOnly));
        if (TryAppendCachedWorld(widgetKey, widgetFingerprint)) continue;
        const std::size_t widgetStartVertex = m_WorldVertices.size();
        const Engine::Vec2F widgetScreen = camera.WorldToScreen(widget.position);
        const float widgetWidth = widget.size.x * zoom;
        const float widgetHeight = widget.size.y * zoom;
        DrawRect(widgetScreen.x, widgetScreen.y, widgetWidth, widgetHeight,
            selectedWidget ? theme.selection[0] * 0.12f : theme.surface[0],
            selectedWidget ? theme.selection[1] * 0.30f : theme.surface[1],
            selectedWidget ? theme.selection[2] * 0.48f : theme.surface[2],
            drawOnly ? 0.45f : 0.96f);
        DrawRect(widgetScreen.x, widgetScreen.y, widgetWidth, 4.0f,
            selectedWidget ? theme.selection[0] : theme.hover[0],
            selectedWidget ? theme.selection[1] : theme.hover[1],
            selectedWidget ? theme.selection[2] : theme.hover[2], 0.95f);
        std::string widgetTitle = widget.title.empty() ? widget.widgetType : widget.title;
        if (workspace.InlineEdit().active && workspace.InlineEdit().targetType == "Widget" &&
            workspace.InlineEdit().targetId == widget.id)
            widgetTitle = workspace.InlineEdit().draft;
        DrawText(widgetScreen.x + 10, widgetScreen.y + 14,
            widgetTitle,
            1.35f, theme.foreground[0], theme.foreground[1], theme.foreground[2]);
        float rowY = widgetScreen.y + 42;
        if (widget.widgetType == "EntityList") {
            const std::string queryId = widget.config.GetString("query_view_id");
            const QueryViewModel* query = nullptr;
            for (const auto& candidate : project.queryViews)
                if (candidate.id == queryId) { query = &candidate; break; }
            if (query != nullptr) {
                for (const auto& entity : query->results) {
                    DrawText(widgetScreen.x + 10, rowY, entity.title, 1.05f,
                        theme.foreground[0], theme.foreground[1], theme.foreground[2]);
                    rowY += 20;
                    if (rowY > widgetScreen.y + widgetHeight - 12) break;
                }
            }
        } else if (widget.widgetType == "SceneLockList") {
            for (const auto& lock : project.sceneLocks) {
                DrawText(widgetScreen.x + 10, rowY,
                    lock.sceneId + " " + lock.status, 1.05f,
                    lock.status == "FREE" ? theme.success[0] : theme.locked[0],
                    lock.status == "FREE" ? theme.success[1] : theme.locked[1],
                    lock.status == "FREE" ? theme.success[2] : theme.locked[2]);
                rowY += 20;
                if (rowY > widgetScreen.y + widgetHeight - 12) break;
            }
        } else if (widget.widgetType == "WorkerList") {
            for (const auto& entity : project.entities) {
                if (entity.type != EntityType::Worker) continue;
                DrawText(widgetScreen.x + 10, rowY, entity.title, 1.05f,
                    theme.foreground[0], theme.foreground[1], theme.foreground[2]);
                rowY += 20;
                if (rowY > widgetScreen.y + widgetHeight - 12) break;
            }
        } else if (widget.widgetType == "RecentActivity") {
            for (const auto& activity : project.activities) {
                DrawText(widgetScreen.x + 10, rowY, activity.summary, 0.95f, 0.78f, 0.84f, 0.92f);
                rowY += 18;
                if (rowY > widgetScreen.y + widgetHeight - 12) break;
            }
        } else {
            DrawText(widgetScreen.x + 10, rowY, widget.config.GetString("text", "READY"),
                1.15f, 0.62f, 0.92f, 0.76f);
        }
        StoreWorldGeometry(widgetKey, widgetFingerprint, widgetStartVertex);
    }
    m_BuildingWorldGeometry = false;

    if (!drawOnly && (space.Current().name == "Overview" || space.Current().name == "Project Overview")) {
        int activeTasks = 0;
        int workerCount = 0;
        int systemCount = 0;
        int lockedScenes = 0;
        std::string milestone = "NONE";
        for (const auto& entity : project.entities) {
            if (entity.type == EntityType::Worker) ++workerCount;
            else if (entity.type == EntityType::System) ++systemCount;
            else if (entity.type == EntityType::Milestone && milestone == "NONE") milestone = entity.title;
            else if (entity.type == EntityType::Task) {
                if (const auto* task = std::get_if<TaskDetails>(&entity.details)) {
                    if (task->status == TaskStatus::InProgress || task->status == TaskStatus::Review ||
                        task->status == TaskStatus::Testing) ++activeTasks;
                }
            }
        }
        for (const auto& lock : project.sceneLocks)
            if (lock.status != "FREE") ++lockedScenes;
        const float panelY = std::max(hudHeight + 12.0f, height - 214.0f);
        DrawRect(16, panelY, 300, 190,
            theme.elevated[0], theme.elevated[1], theme.elevated[2], 0.94f);
        DrawText(28, panelY + 18, "PROJECT OVERVIEW", 1.8f,
            theme.foreground[0], theme.foreground[1], theme.foreground[2]);
        DrawText(28, panelY + 48, "MILESTONE: " + milestone, 1.1f,
            theme.muted[0], theme.muted[1], theme.muted[2]);
        DrawText(28, panelY + 74, "ACTIVE TASKS: " + std::to_string(activeTasks), 1.2f,
            theme.success[0], theme.success[1], theme.success[2]);
        DrawText(28, panelY + 100, "WORKERS: " + std::to_string(workerCount), 1.2f,
            theme.warning[0], theme.warning[1], theme.warning[2]);
        DrawText(28, panelY + 126, "SCENE LOCKS: " + std::to_string(lockedScenes), 1.2f,
            theme.locked[0], theme.locked[1], theme.locked[2]);
        DrawText(28, panelY + 152, "SYSTEMS: " + std::to_string(systemCount), 1.2f,
            theme.accent[0], theme.accent[1], theme.accent[2]);
    }

    // Layer panel is screen-fixed so visibility and lock state remain easy to
    // reach while the canvas is zoomed or panned.
    const float layerPanelWidth = 190.0f;
    if (displayMode != DisplayMode::Presentation) {
        const float layerPanelHeight = std::min(
            height - hudHeight - 16.0f, 70.0f + static_cast<float>(project.userLayers.size()) * 28.0f);
        DrawRect(0, hudHeight, layerPanelWidth, std::max(52.0f, layerPanelHeight),
            theme.surface[0], theme.surface[1], theme.surface[2], 0.94f);
        DrawText(12, hudHeight + 16, "LAYERS", 1.6f, theme.foreground[0], theme.foreground[1], theme.foreground[2], 1.0f);
        for (std::size_t index = 0; index < project.userLayers.size(); ++index) {
            const auto& layer = project.userLayers[index];
            const float rowY = hudHeight + 32.0f + static_cast<float>(index) * 28.0f;
            const bool active = layer.id == workspace.ActiveLayerId();
            DrawRect(6, rowY, layerPanelWidth - 12, 24,
                active ? theme.selection[0] * 0.12f : theme.surface[0],
                active ? theme.selection[1] * 0.34f : theme.surface[1],
                active ? theme.selection[2] * 0.48f : theme.surface[2], 0.96f);
            std::string layerName = layer.name;
            if (workspace.InlineEdit().active && workspace.InlineEdit().targetType == "Layer" &&
                workspace.InlineEdit().targetId == layer.id)
                layerName = workspace.InlineEdit().draft;
            DrawText(12, rowY + 8, layerName, 1.15f,
                layer.locked ? theme.locked[0] : active ? theme.accent[0] : theme.foreground[0],
                layer.locked ? theme.locked[1] : active ? theme.accent[1] : theme.foreground[1],
                layer.locked ? theme.locked[2] : active ? theme.accent[2] : theme.foreground[2], 1.0f);
            DrawText(layerPanelWidth - 46, rowY + 8, layer.locked ? "LOCK" : layer.visible ? "ON" : "OFF",
                0.95f, layer.locked ? theme.locked[0] : theme.muted[0],
                layer.locked ? theme.locked[1] : theme.muted[1],
                layer.locked ? theme.locked[2] : theme.muted[2], 1.0f);
        }
        const float newLayerY = hudHeight + 32.0f + static_cast<float>(project.userLayers.size()) * 28.0f;
        DrawText(12, newLayerY + 8, "+ NEW LAYER", 1.0f,
            theme.success[0], theme.success[1], theme.success[2], 1.0f);
    }
    if (drawOnly && displayMode != DisplayMode::Presentation) {
        DrawRect(204, hudHeight + 8, 350, 28, theme.surface[0], theme.surface[1], theme.surface[2], 0.96f);
        DrawText(214, hudHeight + 17, "PEN HIGHLIGHT LINE ARROW BOX OVAL ERASER", 0.92f,
            theme.accent[0], theme.accent[1], theme.accent[2], 1.0f);
        constexpr std::array<std::array<float, 3>, 7> palette = {{
            {{0.62f, 0.84f, 1.0f}}, {{0.95f, 0.30f, 0.32f}}, {{0.96f, 0.63f, 0.30f}},
            {{0.96f, 0.83f, 0.37f}}, {{0.38f, 0.82f, 0.58f}}, {{0.36f, 0.55f, 0.93f}},
            {{0.64f, 0.47f, 0.84f}}}};
        for (std::size_t index = 0; index < palette.size(); ++index)
            DrawRect(560.0f + static_cast<float>(index) * 26.0f, hudHeight + 8, 20, 20,
                palette[index][0], palette[index][1], palette[index][2], 1.0f);
        DrawText(214, hudHeight + 42, "WIDTH " + std::to_string(static_cast<int>(workspace.Ink().Width())) +
            "   COLOR " + workspace.Ink().Color(), 1.0f, theme.muted[0], theme.muted[1], theme.muted[2], 1.0f);
    }
    const auto& selectionBox = workspace.SelectionRectangle();
    if (displayMode != DisplayMode::Presentation && selectionBox.active) {
        const float left = std::min(selectionBox.start.x, selectionBox.end.x);
        const float right = std::max(selectionBox.start.x, selectionBox.end.x);
        const float top = std::min(selectionBox.start.y, selectionBox.end.y);
        const float bottom = std::max(selectionBox.start.y, selectionBox.end.y);
        DrawRect(left, top, right - left, bottom - top, theme.selection[0], theme.selection[1], theme.selection[2], 0.10f);
        DrawLine({left, top}, {right, top}, 2, theme.selection[0], theme.selection[1], theme.selection[2], 0.85f);
        DrawLine({right, top}, {right, bottom}, 2, theme.selection[0], theme.selection[1], theme.selection[2], 0.85f);
        DrawLine({right, bottom}, {left, bottom}, 2, theme.selection[0], theme.selection[1], theme.selection[2], 0.85f);
        DrawLine({left, bottom}, {left, top}, 2, theme.selection[0], theme.selection[1], theme.selection[2], 0.85f);
    }

    if (displayMode != DisplayMode::Presentation && workspace.Panel() != WorkspacePanel::None) {
        const float panelWidth = 410.0f;
        const float panelX = std::max(layerPanelWidth + 18.0f, worldWidth - panelWidth - 18.0f);
        const float panelY = hudHeight + 64.0f;
        DrawRect(panelX, panelY, panelWidth, 300.0f,
            theme.elevated[0], theme.elevated[1], theme.elevated[2], 0.98f);
        if (workspace.Panel() == WorkspacePanel::QueryBuilder) {
            const auto& query = workspace.QueryDraft();
            DrawText(panelX + 14, panelY + 20, "QUERY BUILDER", 1.8f,
                theme.foreground[0], theme.foreground[1], theme.foreground[2], 1.0f);
            DrawText(panelX + 14, panelY + 48, "ENTITY: " + query.entitySchema, 1.25f,
                theme.accent[0], theme.accent[1], theme.accent[2], 1.0f);
            float rowY = panelY + 78.0f;
            if (query.filters.empty()) {
                DrawText(panelX + 14, rowY, "WHERE  (add filter in draft)", 1.1f,
                    theme.muted[0], theme.muted[1], theme.muted[2], 1.0f);
                rowY += 24.0f;
            } else {
                for (const auto& filter : query.filters) {
                    DrawText(panelX + 14, rowY, filter.property + "  " + filter.op + "  " +
                        JsonText(filter.value), 1.1f, theme.foreground[0], theme.foreground[1], theme.foreground[2], 1.0f);
                    rowY += 22.0f;
                }
            }
            DrawText(panelX + 14, rowY + 4, "LIMIT " + std::to_string(query.resultLimit) +
                "   MATCHES: " + std::to_string(query.results.size()), 1.15f,
                theme.success[0], theme.success[1], theme.success[2], 1.0f);
            rowY += 32.0f;
            for (std::size_t index = 0; index < query.results.size() && index < 5; ++index) {
                DrawText(panelX + 22, rowY, query.results[index].title, 1.05f,
                    theme.muted[0], theme.muted[1], theme.muted[2], 1.0f);
                rowY += 19.0f;
            }
            DrawText(panelX + 14, panelY + 280, "I ADD  O OP  U REMOVE  TYPE VALUE  ENTER SAVE", 0.82f,
                theme.accent[0], theme.accent[1], theme.accent[2], 1.0f);
        } else {
            const auto& schema = workspace.SchemaDraft();
            DrawText(panelX + 14, panelY + 20, "SCHEMA EDITOR", 1.8f,
                theme.foreground[0], theme.foreground[1], theme.foreground[2], 1.0f);
            std::string schemaName = schema.displayName.empty() ? "NO SCHEMA" : schema.displayName;
            if (workspace.InlineEdit().active && workspace.InlineEdit().targetType == "Schema" &&
                workspace.InlineEdit().targetId == schema.id)
                schemaName = workspace.InlineEdit().draft;
            DrawText(panelX + 14, panelY + 48, schemaName,
                1.35f, theme.accent[0], theme.accent[1], theme.accent[2], 1.0f);
            float rowY = panelY + 80.0f;
            for (const auto& property : schema.properties) {
                DrawText(panelX + 14, rowY, property.displayName + "  " + PropertyTypeName(property.type) +
                    (property.required ? "  REQUIRED" : ""), 1.05f,
                    theme.foreground[0], theme.foreground[1], theme.foreground[2], 1.0f);
                rowY += 22.0f;
                if (rowY > panelY + 235.0f) break;
            }
            DrawText(panelX + 14, panelY + 280, "N NEW TEST ASSET  A ADD  E RENAME  X DELETE  ENTER SAVE", 0.78f,
                theme.accent[0], theme.accent[1], theme.accent[2], 1.0f);
        }
    }
    if (displayMode != DisplayMode::Presentation && workspace.ContextMenu().open) {
        const auto& menu = workspace.ContextMenu();
        std::vector<std::string> options;
        if (menu.targetType == "Board") options = {"Rename", "Move To Layer", "Lock / Unlock", "Collapse / Expand", "Duplicate", "Delete"};
        else if (menu.targetType == "Layer") options = {"Rename", "Move Up", "Move Down", "Delete"};
        else if (menu.targetType == "Ink") options = {"Group", "Ungroup", "Duplicate", "Move To Layer", "Delete"};
        else if (menu.targetType == "Widget") options = {"Open Inspector", "Rename", "Move To Layer", "Duplicate", "Delete"};
        else if (menu.targetType == "View") options = {"Open Inspector", "Duplicate View", "Attach To Board", "Detach From Board", "Move To Layer", "Add View To Space", "Delete View"};
        else options = {"Create Note", "Create Board", "Paste"};
        const float menuWidth = 178.0f;
        const float menuHeight = 18.0f + static_cast<float>(options.size()) * 25.0f;
        const float menuX = std::min(menu.screenPosition.x, width - menuWidth - 8.0f);
        const float menuY = std::min(menu.screenPosition.y, height - menuHeight - 8.0f);
        DrawRect(menuX, menuY, menuWidth, menuHeight,
            theme.elevated[0], theme.elevated[1], theme.elevated[2], 0.99f);
        for (std::size_t index = 0; index < options.size(); ++index)
            DrawText(menuX + 10, menuY + 12.0f + static_cast<float>(index) * 25.0f,
                options[index], 1.05f, theme.foreground[0], theme.foreground[1], theme.foreground[2], 1.0f);
    }

    // Inspector is a HUD/system layer and never participates in world zoom.
    if (inspectorWidth > 0) {
        DrawRect(worldWidth, hudHeight, inspectorWidth, height - hudHeight,
            theme.elevated[0], theme.elevated[1], theme.elevated[2], 0.98f);
        DrawText(worldWidth + 16, hudHeight + 18, "INSPECTOR", 2,
            theme.foreground[0], theme.foreground[1], theme.foreground[2]);
        const CrystalEntity* selected = findEntity(workspace.Selection().SelectedId());
        const DashboardWidgetModel* selectedWidget = nullptr;
        for (const auto& widget : project.dashboardWidgets) {
            if (workspace.Selection().IsSelected(
                    Engine::UI2D::SelectionKind::DashboardWidget, widget.id)) {
                selectedWidget = &widget;
                break;
            }
        }
        if (selected == nullptr && selectedWidget == nullptr) {
            DrawText(worldWidth + 16, hudHeight + 54, "SELECT ENTITY", 1.5f,
                theme.disabled[0], theme.disabled[1], theme.disabled[2]);
        } else if (selectedWidget != nullptr) {
            std::string widgetTitle = selectedWidget->title.empty()
                ? selectedWidget->widgetType : selectedWidget->title;
            if (workspace.InlineEdit().active && workspace.InlineEdit().targetType == "Widget" &&
                workspace.InlineEdit().targetId == selectedWidget->id)
                widgetTitle = workspace.InlineEdit().draft;
            DrawText(worldWidth + 16, hudHeight + 52, widgetTitle, 1.7f,
                theme.foreground[0], theme.foreground[1], theme.foreground[2]);
            DrawText(worldWidth + 16, hudHeight + 78,
                "TYPE: " + selectedWidget->widgetType, 1.2f,
                theme.accent[0], theme.accent[1], theme.accent[2]);
            DrawText(worldWidth + 16, hudHeight + 104,
                "SIZE: " + std::to_string(static_cast<int>(selectedWidget->size.x)) + " x " +
                    std::to_string(static_cast<int>(selectedWidget->size.y)), 1.15f,
                theme.success[0], theme.success[1], theme.success[2]);
            DrawText(worldWidth + 16, hudHeight + 130,
                "LAYER: " + selectedWidget->layerId + "  REV " +
                    std::to_string(selectedWidget->revision), 1.1f,
                theme.muted[0], theme.muted[1], theme.muted[2]);
            DrawText(worldWidth + 16, hudHeight + 166, "CONFIG", 1.2f,
                theme.accent[0], theme.accent[1], theme.accent[2]);
            DrawText(worldWidth + 16, hudHeight + 190, selectedWidget->config.Dump(), 0.82f,
                theme.foreground[0], theme.foreground[1], theme.foreground[2]);
            DrawText(worldWidth + 16, hudHeight + 238, "E RENAME   DRAG CORNER RESIZE", 0.95f,
                theme.warning[0], theme.warning[1], theme.warning[2]);
        } else {
            DrawText(worldWidth + 16, hudHeight + 52, selected->title, 1.8f,
                theme.foreground[0], theme.foreground[1], theme.foreground[2]);
            DrawText(worldWidth + 16, hudHeight + 76, EntityTypeName(selected->type), 1.3f,
                theme.accent[0], theme.accent[1], theme.accent[2]);
            DrawText(worldWidth + 16, hudHeight + 104, "REVISION " + std::to_string(selected->revision), 1.2f,
                theme.muted[0], theme.muted[1], theme.muted[2]);
            if (!selected->description.empty()) DrawText(worldWidth + 16, hudHeight + 132, selected->description, 1.1f,
                theme.muted[0], theme.muted[1], theme.muted[2]);
            float y = hudHeight + 170;
            if (const auto* task = std::get_if<TaskDetails>(&selected->details)) {
                DrawText(worldWidth + 16, y, "STATUS: " + std::string(TaskStatusName(task->status)), 1.3f, 0.62f, 0.90f, 1.0f); y += 24;
                DrawText(worldWidth + 16, y, "PRIORITY: " + std::string(TaskPriorityName(task->priority)), 1.3f, 1.0f, 0.72f, 0.48f); y += 24;
                DrawText(worldWidth + 16, y, "OWNER: " + ownerName(task->assignedWorkerId), 1.2f, 0.90f, 0.82f, 0.70f); y += 24;
                DrawText(worldWidth + 16, y, "BRANCH: " + task->branch, 1.1f, 0.74f, 0.82f, 0.92f); y += 24;
                DrawText(worldWidth + 16, y, "ACCEPTANCE", 1.2f, 0.74f, 0.86f, 0.94f); y += 22;
                DrawText(worldWidth + 16, y, task->acceptanceCriteria, 1.0f, 0.68f, 0.74f, 0.84f);
            } else if (const auto* worker = std::get_if<WorkerDetails>(&selected->details)) {
                DrawText(worldWidth + 16, y, "TYPE: " + std::string(WorkerTypeName(worker->type)), 1.2f, 0.78f, 0.86f, 1.0f); y += 24;
                DrawText(worldWidth + 16, y, "STATUS: " + std::string(WorkerStatusName(worker->status)), 1.2f, 0.62f, 0.92f, 0.74f); y += 24;
                DrawText(worldWidth + 16, y, "TASK: " + (worker->currentTask.empty() ? std::string("NONE") : worker->currentTask), 1.1f, 0.90f, 0.82f, 0.70f); y += 24;
                DrawText(worldWidth + 16, y, "BRANCH: " + worker->branch, 1.1f, 0.74f, 0.82f, 0.92f); y += 24;
                DrawText(worldWidth + 16, y, "LOCKS: " + std::to_string(worker->currentLockIds.size()), 1.1f, 0.92f, 0.78f, 0.62f);
            } else if (const auto* system = std::get_if<SystemDetails>(&selected->details)) {
                DrawText(worldWidth + 16, y, "STATUS: " + std::string(SystemStatusName(system->status)), 1.2f, 0.76f, 0.88f, 1.0f); y += 24;
                DrawText(worldWidth + 16, y, "PROGRESS: " + std::to_string(static_cast<int>(system->progress * 100.0)) + "%", 1.2f, 0.62f, 0.92f, 0.72f);
            } else if (const auto* scene = std::get_if<SceneDetails>(&selected->details)) {
                DrawText(worldWidth + 16, y, "ASSET: " + scene->assetPath, 1.1f, 0.74f, 0.84f, 0.94f);
                DrawText(worldWidth + 16, y + 28, "LOCK: " + workspace.CurrentSceneStatus(selected->id), 1.2f, 0.92f, 0.76f, 0.62f);
            } else if (const auto* milestone = std::get_if<MilestoneDetails>(&selected->details)) {
                DrawText(worldWidth + 16, y, "STATUS: " + std::string(MilestoneStatusName(milestone->status)), 1.2f, 0.76f, 0.88f, 1.0f); y += 24;
                DrawText(worldWidth + 16, y, "PROGRESS: " + std::to_string(static_cast<int>(milestone->progress * 100.0)) + "%", 1.2f, 0.62f, 0.92f, 0.72f); y += 24;
                DrawText(worldWidth + 16, y, "TARGET: " + milestone->targetDate, 1.1f, 0.82f, 0.84f, 0.94f);
            } else if (selected->type == EntityType::Unknown) {
                const CrystalEntitySchema* schema = nullptr;
                for (const auto& candidate : project.schemas)
                    if (candidate.id == selected->schemaId) { schema = &candidate; break; }
                DrawText(worldWidth + 16, y,
                    schema == nullptr ? selected->typeName : schema->displayName,
                    1.35f, 0.72f, 0.86f, 1.0f);
                y += 28;
                if (schema != nullptr) {
                    for (const auto& definition : schema->properties) {
                        const Engine::Json::Value* property = selected->properties.Find(definition.name);
                        if (property == nullptr) continue;
                        DrawText(worldWidth + 16, y,
                            definition.displayName + ": " + JsonText(*property),
                            1.05f, 0.78f, 0.88f, 0.94f);
                        if (definition.type == PropertyType::Bool) {
                            DrawRect(worldWidth + inspectorWidth - 48, y - 2, 22, 12,
                                property->IsBool() && property->AsBool() ? 0.30f : 0.16f,
                                property->IsBool() && property->AsBool() ? 0.82f : 0.20f,
                                0.42f, 0.9f);
                        } else if (definition.type == PropertyType::Progress && property->IsNumber()) {
                            const float progress = std::clamp(
                                static_cast<float>(property->AsNumber()) / 100.0f, 0.0f, 1.0f);
                            DrawRect(worldWidth + 16, y + 13, inspectorWidth - 32, 5,
                                0.12f, 0.18f, 0.24f, 0.95f);
                            DrawRect(worldWidth + 16, y + 13, (inspectorWidth - 32) * progress, 5,
                                0.30f, 0.78f, 0.58f, 0.95f);
                            y += 6;
                        }
                        y += 22;
                        if (y > height - 90) break;
                    }
                }
            }
        }
        DrawText(worldWidth + 16, height - 66, "CLICK FIELD THEN TYPE", 1.0f,
            theme.disabled[0], theme.disabled[1], theme.disabled[2]);
    }
    if (displayMode != DisplayMode::Presentation && workspace.PropertyPicker().open) {
        constexpr float popupWidth = 274.0f;
        constexpr float popupHeight = 240.0f;
        const float popupX = width - popupWidth - 14.0f;
        const float popupY = std::min(190.0f, height - popupHeight - 12.0f);
        DrawRect(popupX, popupY, popupWidth, popupHeight,
            theme.elevated[0], theme.elevated[1], theme.elevated[2], 0.99f);
        DrawText(popupX + 12, popupY + 18, "ENTITY PICKER", 1.45f,
            theme.foreground[0], theme.foreground[1], theme.foreground[2], 1.0f);
        DrawRect(popupX + 10, popupY + 25, popupWidth - 20, 24,
            theme.surface[0], theme.surface[1], theme.surface[2], 1.0f);
        DrawText(popupX + 18, popupY + 34,
            workspace.PropertyPicker().filter.empty() ? "Search Entity..." : workspace.PropertyPicker().filter,
            1.0f, theme.muted[0], theme.muted[1], theme.muted[2], 1.0f);
        std::string filter = workspace.PropertyPicker().filter;
        std::transform(filter.begin(), filter.end(), filter.begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        int row = 0;
        for (const auto& entity : project.entities) {
            if (std::find(workspace.PropertyPicker().entityIds.begin(),
                    workspace.PropertyPicker().entityIds.end(), entity.id) ==
                workspace.PropertyPicker().entityIds.end()) continue;
            std::string haystack = entity.title + " " + std::string(EntityTypeName(entity.type)) + " " + entity.typeName;
            std::transform(haystack.begin(), haystack.end(), haystack.begin(), [](unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
            if (!filter.empty() && haystack.find(filter) == std::string::npos) continue;
            if (row >= 6) break;
            DrawText(popupX + 16, popupY + 62 + static_cast<float>(row) * 28.0f,
                entity.title + "  " + std::string(EntityTypeName(entity.type)), 1.0f,
                theme.foreground[0], theme.foreground[1], theme.foreground[2], 1.0f);
            ++row;
        }
    }
    if (!workspace.Notification().empty()) {
        const float notificationWidth = std::max(120.0f, std::min(480.0f, worldWidth - 190.0f));
        DrawRect(16, height - 42, notificationWidth, 26,
            theme.error[0] * 0.35f, theme.error[1] * 0.35f, theme.error[2] * 0.35f, 0.94f);
        DrawText(28, height - 34, workspace.Notification(), 1.1f,
            theme.warning[0], theme.warning[1], theme.warning[2]);
    }
    m_CachedHudVertices = m_Vertices;
    m_CachedWorldVertices = m_WorldVertices;
    m_CachedFramebufferWidth = m_FramebufferWidth;
    m_CachedFramebufferHeight = m_FramebufferHeight;
    m_GeometryCacheValid = true;
    if (cacheWasInvalid)
        ++m_GeometryBuildCount;
    m_HasRenderedCamera = true;
    m_LastRenderedCameraCenterX = m_CameraCenterX;
    m_LastRenderedCameraCenterY = m_CameraCenterY;
    m_LastRenderedCameraZoom = m_CameraZoom;
    m_LastRenderedViewportWidth = m_CameraViewportWidth;
    m_LastRenderedViewportHeight = m_CameraViewportHeight;
    m_BuildingWorldGeometry = false;
    DrawRect(worldWidth - 148, height - 42, 132, 26,
        theme.surface[0], theme.surface[1], theme.surface[2], 0.94f);
    DrawText(worldWidth - 136, height - 34,
        std::to_string(static_cast<int>(std::round(currentCamera.zoom * 100.0))) + "%", 2,
        0.80f, 0.90f, 1.0f);
    Flush(m_FramebufferWidth, m_FramebufferHeight);
}

} // namespace Crystal
