#include "mini/render/Texture.hpp"
#include "mini/platform/OpenGL.hpp"

#include <stb_image.h>

#include <iostream>
#include <utility>
#include <vector>

namespace mini
{

Texture::Texture(const unsigned char* data, int width, int height, int channels)
    : m_width(width), m_height(height)
{
    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    const GLenum fmt = (channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(fmt),
                 width, height, 0, fmt, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    std::cout << "[Texture] Creata " << width << "x" << height
              << " (id=" << m_id << ", ch=" << channels << ")" << std::endl;
}

Texture::~Texture()
{ if (m_id != 0) { glDeleteTextures(1, &m_id); m_id = 0; } }

Texture::Texture(Texture&& o) noexcept
    : m_id(o.m_id), m_width(o.m_width), m_height(o.m_height), m_path(std::move(o.m_path))
{ o.m_id = 0; }

Texture& Texture::operator=(Texture&& o) noexcept
{
    if (this != &o) {
        if (m_id != 0) glDeleteTextures(1, &m_id);
        m_id = o.m_id; m_width = o.m_width; m_height = o.m_height;
        m_path = std::move(o.m_path); o.m_id = 0;
    }
    return *this;
}

void Texture::bind(unsigned int slot) const
{ glActiveTexture(GL_TEXTURE0 + slot); glBindTexture(GL_TEXTURE_2D, m_id); }

void Texture::unbind() const { glBindTexture(GL_TEXTURE_2D, 0); }
unsigned int       Texture::getId()     const { return m_id; }
int                Texture::getWidth()  const { return m_width; }
int                Texture::getHeight() const { return m_height; }
const std::string& Texture::getPath()   const { return m_path; }

// ============================================================
// Caricamento da file (stb_image)
// ============================================================

/*static*/ std::optional<Texture> Texture::loadFromFile(const char* path, bool flipY)
{
    stbi_set_flip_vertically_on_load(flipY ? 1 : 0);
    int w = 0, h = 0, ch = 0;
    unsigned char* data = stbi_load(path, &w, &h, &ch, 0);
    if (!data)
    {
        std::cerr << "[Texture] Non trovato: '" << path
                  << "' — " << stbi_failure_reason() << std::endl;
        return std::nullopt;
    }
    std::cout << "[Texture] Caricata '" << path << "' "
              << w << "x" << h << " ch=" << ch << std::endl;
    Texture tex(data, w, h, ch);
    tex.m_path = path;
    stbi_image_free(data);
    return tex;
}

// ============================================================
// Scacchiera procedurale (fallback)
// ============================================================

/*static*/ Texture Texture::createCheckerboard(int size, int cellSize)
{
    std::vector<unsigned char> data;
    data.reserve(static_cast<std::size_t>(size * size * 3));
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
        {
            const bool bright = ((x / cellSize) + (y / cellSize)) % 2 == 0;
            const unsigned char v = bright ? 210u : 60u;
            data.push_back(v); data.push_back(v); data.push_back(v);
        }
    Texture tex(data.data(), size, size, 3);
    tex.m_path = "<checkerboard>";
    return tex;
}

} // namespace mini