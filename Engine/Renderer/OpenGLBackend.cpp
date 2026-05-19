#include "OpenGLBackend.h"

#include "Core/Log.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

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
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
uniform mat4 u_ViewProj;
out vec2 vUV;

void main() {
    vUV = aUV;
    gl_Position = u_ViewProj * vec4(aPos, 1.0);
}
)";

    constexpr const char* kFragmentShaderSrc = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D u_MainTex;
uniform int u_HasTexture;
uniform vec4 u_Tint;

void main() {
    vec4 baseColor = (u_HasTexture != 0) ? texture(u_MainTex, vUV) : vec4(0.12, 0.68, 0.96, 1.0);
    FragColor = baseColor * u_Tint;
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

    m_ViewProjLocation = glGetUniformLocation(m_ShaderProgram, "u_ViewProj");
    m_HasTextureLocation = glGetUniformLocation(m_ShaderProgram, "u_HasTexture");
    m_TintLocation = glGetUniformLocation(m_ShaderProgram, "u_Tint");
    const int samplerLocation = glGetUniformLocation(m_ShaderProgram, "u_MainTex");
    if (m_ViewProjLocation < 0 || m_HasTextureLocation < 0 || m_TintLocation < 0 || samplerLocation < 0) {
        XLOG_ERROR("Failed to find required shader uniforms");
        return false;
    }

    glUseProgram(m_ShaderProgram);
    glUniform1i(samplerLocation, 0);
    glUniform1i(m_HasTextureLocation, 0);
    glUniform4fv(m_TintLocation, 1, m_Tint);
    glUseProgram(0);

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), static_cast<void*>(nullptr));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

void OpenGLBackend::SetViewport(int width, int height) {
    glViewport(0, 0, width, height);
}

void OpenGLBackend::UploadMesh(
    const float* vertices,
    int vertexCount,
    int vertexDimension,
    const float* uvs,
    int uvCount,
    const unsigned int* indices,
    int indexCount
) {
    if (vertices == nullptr || indices == nullptr || vertexCount < 3 || indexCount < 3 || (indexCount % 3) != 0) {
        return;
    }

    if (vertexDimension != 2 && vertexDimension != 3) {
        return;
    }

    float minX = vertices[0];
    float maxX = vertices[0];
    float minY = vertices[1];
    float maxY = vertices[1];

    for (int i = 0; i < vertexCount; ++i) {
        const float x = vertices[i * vertexDimension];
        const float y = vertices[i * vertexDimension + 1];
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }

    float rangeX = maxX - minX;
    float rangeY = maxY - minY;
    if (std::fabs(rangeX) < 1e-6f) {
        rangeX = 1.0f;
    }
    if (std::fabs(rangeY) < 1e-6f) {
        rangeY = 1.0f;
    }

    const bool hasExplicitUVs = (uvs != nullptr && uvCount == vertexCount);

    std::vector<float> interleaved;
    interleaved.reserve(static_cast<std::size_t>(vertexCount) * 5);

    for (int i = 0; i < vertexCount; ++i) {
        const float x = vertices[i * vertexDimension];
        const float y = vertices[i * vertexDimension + 1];
        const float z = (vertexDimension == 3) ? vertices[i * vertexDimension + 2] : 0.0f;
        const float u = hasExplicitUVs ? uvs[static_cast<std::size_t>(i) * 2] : ((x - minX) / rangeX);
        const float v = hasExplicitUVs ? uvs[static_cast<std::size_t>(i) * 2 + 1] : ((y - minY) / rangeY);

        interleaved.push_back(x);
        interleaved.push_back(y);
        interleaved.push_back(z);
        interleaved.push_back(u);
        interleaved.push_back(v);
    }

    for (int i = 0; i < indexCount; ++i) {
        if (indices[i] >= static_cast<unsigned int>(vertexCount)) {
            return;
        }
    }

    if (m_VAO == 0 || m_VBO == 0 || m_EBO == 0) {
        return;
    }

    m_IndexCount = indexCount;

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(sizeof(float) * interleaved.size()),
        interleaved.data(),
        GL_DYNAMIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(unsigned int) * indexCount), indices, GL_DYNAMIC_DRAW);

    glBindVertexArray(0);
}

void OpenGLBackend::UploadTextureRGBA8(const unsigned char* pixels, int width, int height) {
    if (pixels == nullptr || width <= 0 || height <= 0) {
        ClearTexture();
        return;
    }

    if (m_MainTexture == 0) {
        glGenTextures(1, &m_MainTexture);
    }

    glBindTexture(GL_TEXTURE_2D, m_MainTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels
    );
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    m_HasTexture = true;
}

void OpenGLBackend::ClearTexture() {
    m_HasTexture = false;
}

void OpenGLBackend::SetMaterialTint(const float* rgba) {
    if (rgba == nullptr) {
        m_Tint[0] = 1.0f;
        m_Tint[1] = 1.0f;
        m_Tint[2] = 1.0f;
        m_Tint[3] = 1.0f;
        return;
    }

    std::memcpy(m_Tint, rgba, sizeof(float) * 4);
}

void OpenGLBackend::SetViewProjection(const float* matrix4x4) {
    if (matrix4x4 == nullptr || m_ViewProjLocation < 0 || m_ShaderProgram == 0) {
        return;
    }

    glUseProgram(m_ShaderProgram);
    glUniformMatrix4fv(m_ViewProjLocation, 1, GL_FALSE, matrix4x4);
    glUseProgram(0);
}

void OpenGLBackend::BeginFrame() {
    glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLBackend::DrawMesh() {
    if (m_IndexCount <= 0) {
        return;
    }

    glUseProgram(m_ShaderProgram);
    glUniform4fv(m_TintLocation, 1, m_Tint);

    if (m_HasTexture && m_MainTexture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_MainTexture);
        glUniform1i(m_HasTextureLocation, 1);
    } else {
        glUniform1i(m_HasTextureLocation, 0);
    }

    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, m_IndexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    if (m_HasTexture && m_MainTexture != 0) {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glUseProgram(0);
}

void OpenGLBackend::EndFrame() {}

void OpenGLBackend::Shutdown() {
    m_IndexCount = 0;
    m_HasTexture = false;

    if (m_MainTexture != 0) {
        glDeleteTextures(1, &m_MainTexture);
        m_MainTexture = 0;
    }

    if (m_EBO != 0) {
        glDeleteBuffers(1, &m_EBO);
        m_EBO = 0;
    }

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
