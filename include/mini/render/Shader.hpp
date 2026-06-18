#pragma once

namespace mini
{

// Compila e linka uno shader program GLSL.
// Gestisce la vita del program ID e offre utility per impostare uniform.
class Shader
{
public:
    // vertSrc e fragSrc sono stringhe GLSL raw (non file path).
    // Il costruttore lancia std::runtime_error se la compilazione fallisce.
    Shader(const char* vertSrc, const char* fragSrc);
    ~Shader();

    Shader(const Shader&)            = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const;

    [[nodiscard]] unsigned int getId()    const;
    [[nodiscard]] bool         isValid()  const;

    // Uniform setters — richiedono che lo shader sia attivo (use() chiamato)
    void setFloat (const char* name, float value)                           const;
    void setVec2  (const char* name, float x, float y)                     const;
    void setVec3  (const char* name, float x, float y, float z)            const;
    void setVec4  (const char* name, float x, float y, float z, float w)   const;
    void setMat4  (const char* name, const float* matrix)                  const;

private:
    unsigned int m_id    = 0;
    bool         m_valid = false;

    // Compila un singolo stage (GL_VERTEX_SHADER / GL_FRAGMENT_SHADER)
    static unsigned int compileStage(unsigned int type, const char* src);
};

} // namespace mini