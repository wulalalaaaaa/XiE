#include "Renderer.h"

#include "Core/Log.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

namespace Engine {

<<<<<<< Updated upstream
namespace {

unsigned int CompileShader(unsigned int type, const char* source) {
    const unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == 0) {
        char infoLog[1024] = {0};
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        XLOG_ERROR(infoLog);
        glDeleteShader(shader);
        return 0;
=======
template <bool Debug>
bool BasicRenderer<Debug>::Init(
    IRenderSurface& surface,
    const std::filesystem::path& meshFilePath,
    const std::filesystem::path& textureFilePath,
    const std::filesystem::path& materialFilePath,
    const std::filesystem::path& spriteFilePath,
    int uvMode
) {
    m_Surface = &surface;
    m_MeshFilePath = meshFilePath;
    m_TextureFilePath = textureFilePath;
    m_MaterialFilePath = materialFilePath;
    m_SpriteFilePath = spriteFilePath;
    m_UVMode = uvMode;

    m_Backend = std::make_unique<OpenGLBackend>();
    m_Surface->MakeCurrent();
    if (!m_Backend->Init()) {
        XLOG_ERROR("Failed to initialize render backend");
        return false;
>>>>>>> Stashed changes
    }

    return shader;
}

} // namespace

bool Renderer::Init(GLFWwindow* windowHandle) {
    if (!InitOpenGLFunctions(windowHandle)) {
        return false;
    }

<<<<<<< Updated upstream
    if (!CreateTrianglePipeline()) {
        return false;
    }

    glViewport(0, 0, 1280, 720);
    return true;
}

bool Renderer::InitOpenGLFunctions(GLFWwindow* windowHandle) {
    if (windowHandle == nullptr) {
        XLOG_ERROR("Renderer::InitOpenGLFunctions got null window");
        return false;
    }

    if (!gladLoadGL(glfwGetProcAddress)) {
        XLOG_ERROR("Failed to initialize GLAD");
=======
    if (m_Surface == nullptr || !m_Feature->Init(*m_Backend, *m_Surface)) {
        XLOG_ERROR("Failed to initialize render feature");
>>>>>>> Stashed changes
        return false;
    }

    return true;
}

<<<<<<< Updated upstream
bool Renderer::CreateTrianglePipeline() {
    constexpr const char* kVertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
=======
template <bool Debug>
void BasicRenderer<Debug>::Tick(float dt) {
    if (m_Feature && m_Backend && m_Surface) {
        m_Surface->MakeCurrent();
        m_Feature->OnUpdate(*m_Backend, *m_Surface, dt);
    }
>>>>>>> Stashed changes
}
)";

<<<<<<< Updated upstream
    constexpr const char* kFragmentShaderSrc = R"(
#version 330 core
out vec4 FragColor;

void main() {
    FragColor = vec4(0.12, 0.68, 0.96, 1.0);
=======
template <bool Debug>
void BasicRenderer<Debug>::BeginFrame() {
    if (m_Feature && m_Backend && m_Surface) {
        m_Surface->MakeCurrent();
        m_Feature->OnRender(*m_Backend, *m_Surface);
    }
>>>>>>> Stashed changes
}
)";

<<<<<<< Updated upstream
    const unsigned int vertexShader = CompileShader(GL_VERTEX_SHADER, kVertexShaderSrc);
    if (vertexShader == 0) {
        return false;
    }

    const unsigned int fragmentShader = CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderSrc);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
=======
template <bool Debug>
void BasicRenderer<Debug>::EndFrame() {}

template <bool Debug>
void BasicRenderer<Debug>::Shutdown() {
    if (m_Feature && m_Backend) {
        m_Feature->Shutdown(*m_Backend);
        m_Feature.reset();
    }
    m_Feature2D = nullptr;

    if (m_Backend) {
        m_Backend->Shutdown();
        m_Backend.reset();
    }

    m_Surface = nullptr;
}

template <bool Debug>
bool BasicRenderer<Debug>::SubmitRuntimeMesh2D(
    const float* vertices,
    int vertexCount,
    int vertexDimension,
    const float* uvs,
    int uvCount,
    const unsigned int* indices,
    int indexCount
) {
    if (m_Feature2D == nullptr) {
        return false;
    }

    return m_Feature2D->SubmitRuntimeMesh(vertices, vertexCount, vertexDimension, uvs, uvCount, indices, indexCount);
}

template <bool Debug>
void BasicRenderer<Debug>::SubmitDrawList2D(const DrawList2D& drawList) {
    if (m_Feature2D == nullptr) {
        return;
    }

    m_Feature2D->SubmitDrawList(drawList);
}

template <bool Debug>
void BasicRenderer<Debug>::ClearRuntimeMesh2D() {
    if (m_Feature2D == nullptr) {
        return;
>>>>>>> Stashed changes
    }

    m_ShaderProgram = glCreateProgram();
    glAttachShader(m_ShaderProgram, vertexShader);
    glAttachShader(m_ShaderProgram, fragmentShader);
    glLinkProgram(m_ShaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    int success = 0;
    glGetProgramiv(m_ShaderProgram, GL_LINK_STATUS, &success);
    if (success == 0) {
        char infoLog[1024] = {0};
        glGetProgramInfoLog(m_ShaderProgram, 1024, nullptr, infoLog);
        XLOG_ERROR(infoLog);

        glDeleteProgram(m_ShaderProgram);
        m_ShaderProgram = 0;
        return false;
    }

    constexpr float kVertices[] = {
         0.0f,  0.6f,
        -0.6f, -0.6f,
         0.6f, -0.6f,
    };

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kVertices), kVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), static_cast<void*>(nullptr));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

<<<<<<< Updated upstream
void Renderer::BeginFrame() {
    glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(m_ShaderProgram);
    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glUseProgram(0);
}

void Renderer::EndFrame() {}

void Renderer::Shutdown() {
    if (m_VBO != 0) {
        glDeleteBuffers(1, &m_VBO);
        m_VBO = 0;
    }

    if (m_VAO != 0) {
        glDeleteVertexArrays(1, &m_VAO);
        m_VAO = 0;
    }

    if (m_ShaderProgram != 0) {
        glDeleteProgram(m_ShaderProgram);
        m_ShaderProgram = 0;
    }
}
=======
template <bool Debug>
void BasicRenderer<Debug>::SetCamera2D(float centerX, float centerY, float zoom) {
    if (m_Feature2D == nullptr) {
        return;
    }

    m_Feature2D->SetCamera(centerX, centerY, zoom);
}

// explicit template instantiations for DLL build
template class BasicRenderer<true>;
template class BasicRenderer<false>;
>>>>>>> Stashed changes

} // namespace Engine
