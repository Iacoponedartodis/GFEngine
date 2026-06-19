#include "mini/render/Shader.hpp"
#include "mini/platform/OpenGL.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace mini
{

unsigned int Shader::compileStage(unsigned int type, const char* src)
{
    const GLuint shader = glCreateShader(static_cast<GLenum>(type));
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        GLint logLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<GLchar> log(static_cast<std::size_t>(logLen));
        glGetShaderInfoLog(shader, logLen, nullptr, log.data());
        const std::string stage = (type == GL_VERTEX_SHADER) ? "vertex" : "fragment";
        glDeleteShader(shader);
        throw std::runtime_error("[Shader] Errore compilazione " + stage + ":\n" + log.data());
    }
    return static_cast<unsigned int>(shader);
}

Shader::Shader(const char* vertSrc, const char* fragSrc)
{
    const unsigned int vert = compileStage(GL_VERTEX_SHADER,   vertSrc);
    const unsigned int frag = compileStage(GL_FRAGMENT_SHADER, fragSrc);

    m_id = static_cast<unsigned int>(glCreateProgram());
    glAttachShader(m_id, vert);
    glAttachShader(m_id, frag);
    glLinkProgram(m_id);

    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint ok = 0;
    glGetProgramiv(m_id, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        GLint logLen = 0;
        glGetProgramiv(m_id, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<GLchar> log(static_cast<std::size_t>(logLen));
        glGetProgramInfoLog(m_id, logLen, nullptr, log.data());
        glDeleteProgram(m_id);
        m_id = 0;
        throw std::runtime_error(std::string("[Shader] Errore link:\n") + log.data());
    }

    m_valid = true;
    std::cout << "[Shader] Program " << m_id << " compilato e linkato." << std::endl;
}

Shader::~Shader()
{
    if (m_id != 0) glDeleteProgram(m_id);
}

void Shader::use()    const { glUseProgram(m_id); }
unsigned int Shader::getId()    const { return m_id; }
bool         Shader::isValid()  const { return m_valid; }

void Shader::setInt  (const char* n, int v)                          const { glUniform1i(glGetUniformLocation(m_id, n), v); }
void Shader::setFloat(const char* n, float v)                        const { glUniform1f(glGetUniformLocation(m_id, n), v); }
void Shader::setVec2 (const char* n, float x, float y)              const { glUniform2f(glGetUniformLocation(m_id, n), x, y); }
void Shader::setVec3 (const char* n, float x, float y, float z)     const { glUniform3f(glGetUniformLocation(m_id, n), x, y, z); }
void Shader::setVec4 (const char* n, float x, float y, float z, float w) const { glUniform4f(glGetUniformLocation(m_id, n), x, y, z, w); }
void Shader::setMat4 (const char* n, const float* m)                 const { glUniformMatrix4fv(glGetUniformLocation(m_id, n), 1, GL_FALSE, m); }

} // namespace mini