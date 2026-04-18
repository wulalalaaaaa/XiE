#include "OpenGLBackend.h"

#include "Core/Log.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

namespace Engine {

unsigned int OpenGLBackend::CompileShader(unsigned int type, const char* source) {
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
    }

    return shader;
}

bool OpenGLBackend::Init(GLFWwindow* windowHandle) {
    if (windowHandle == nullptr) {
        XLOG_ERROR("OpenGLBackend::Init got null window");
        return false;
    }

    if (!gladLoadGL(glfwGetProcAddress)) {
        XLOG_ERROR("Failed to initialize GLAD");
        return false;
    }

    return CreateTrianglePipeline();
}

bool OpenGLBackend::CreateTrianglePipeline() {
    constexpr const char* kVertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

    constexpr const char* kFragmentShaderSrc = R"(
#version 330 core
out vec4 FragColor;

void main() {
    FragColor = vec4(0.12, 0.68, 0.96, 1.0);
}
)";

    const unsigned int vertexShader = CompileShader(GL_VERTEX_SHADER, kVertexShaderSrc);
    if (vertexShader == 0) {
        return false;
    }

    const unsigned int fragmentShader = CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderSrc);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
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

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), static_cast<void*>(nullptr));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

void OpenGLBackend::SetViewport(int width, int height) {
    glViewport(0, 0, width, height);
}

void OpenGLBackend::UploadTriangleVertices(const float* vertices, int floatCount) {
    if (vertices == nullptr || floatCount != 6) {
        return;
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * 6, vertices);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void OpenGLBackend::BeginFrame() {
    glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLBackend::DrawTriangle() {
    glUseProgram(m_ShaderProgram);
    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glUseProgram(0);
}

void OpenGLBackend::EndFrame() {}

void OpenGLBackend::Shutdown() {
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

} // namespace Engine
