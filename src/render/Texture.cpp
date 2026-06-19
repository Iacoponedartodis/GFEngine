#include "mini/render/Texture.hpp"
#include "mini/platform/OpenGL.hpp"

#include <iostream>
#include <utility>

namespace mini
{

Texture::Texture(const unsigned char* data, int width, int height, int channels)
    : m_width(width)
    , m_height(height)
{
    // glGenTextures e glBindTexture sono GL 1.0 — in opengl32.dll direttamente
    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);

    // Wrapping: ripeti la texture se UV escono da [0,1]
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Filtering: trilineare per qualita' ottimale con mipmap
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    const GLenum fmt = (channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(fmt),
                 width, height, 0, fmt, GL_UNSIGNED_BYTE, data);

    // Genera mipmap automaticamente
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);

    std::cout << "[Texture] Creata " << width << "x" << height
              << " (id=" << m_id << ", ch=" << channels << ")" << std::endl;
}

Texture::~Texture()
{
    if (m_id != 0)
    {
        glDeleteTextures(1, &m_id);
        m_id = 0;
    }
}

Texture::Texture(Texture&& other) noexcept
    : m_id(other.m_id)
    , m_width(other.m_width)
    , m_height(other.m_height)
{
    other.m_id = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept
{
    if (this != &other)
    {
        if (m_id != 0) glDeleteTextures(1, &m_id);
        m_id     = other.m_id;
        m_width  = other.m_width;
        m_height = other.m_height;
        other.m_id = 0;
    }
    return *this;
}

void Texture::bind(unsigned int slot) const
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_id);
}

void Texture::unbind() const
{
    glBindTexture(GL_TEXTURE_2D, 0);
}

unsigned int Texture::getId()     const { return m_id; }
int          Texture::getWidth()  const { return m_width; }
int          Texture::getHeight() const { return m_height; }

// ============================================================
// Factory: scacchiera bicolore
// ============================================================

/*static*/ Texture Texture::createCheckerboard(int size, int cellSize)
{
    std::vector<unsigned char> data;
    data.reserve(static_cast<std::size_t>(size * size * 3));

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            // Celle alternate chiare/scure
            const bool bright = ((x / cellSize) + (y / cellSize)) % 2 == 0;
            const unsigned char v = bright ? 210u : 60u;
            data.push_back(v);
            data.push_back(v);
            data.push_back(v);
        }
    }

    return Texture(data.data(), size, size, 3);
}

} // namespace mini