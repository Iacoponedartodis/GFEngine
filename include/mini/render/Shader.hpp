#pragma once

namespace mini
{

class Shader
{
public:
    Shader(const char* vertSrc, const char* fragSrc);
    ~Shader();

    Shader(const Shader&)            = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const;

    [[nodiscard]] unsigned int getId()   const;
    [[nodiscard]] bool         isValid() const;

    // Uniform setters (lo shader deve essere attivo con use() prima di chiamarli)
    void setInt  (const char* name, int   value)                           const;
    void setFloat(const char* name, float value)                           const;
    void setVec2 (const char* name, float x, float y)                     const;
    void setVec3 (const char* name, float x, float y, float z)            const;
    void setVec4 (const char* name, float x, float y, float z, float w)   const;
    void setMat4 (const char* name, const float* matrix)                  const;

private:
    unsigned int m_id    = 0;
    bool         m_valid = false;

    static unsigned int compileStage(unsigned int type, const char* src);
};

} // namespace mini