#pragma once

#include <vector>

namespace mini
{

// Wrapper RAII per una texture OpenGL 2D.
// Supporta creazione da dati raw e factory procedurali.
// Il file loading (stb_image) verra' aggiunto quando servira' caricare asset reali.
class Texture
{
public:
    // Crea una texture da dati raw in memoria (RGB o RGBA)
    Texture(const unsigned char* data, int width, int height, int channels = 3);
    ~Texture();

    Texture(const Texture&)            = delete;
    Texture& operator=(const Texture&) = delete;

    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    // Attiva la texture sullo slot indicato (0 = GL_TEXTURE0, 1 = GL_TEXTURE1, ...)
    void bind(unsigned int slot = 0) const;
    void unbind() const;

    [[nodiscard]] unsigned int getId()     const;
    [[nodiscard]] int          getWidth()  const;
    [[nodiscard]] int          getHeight() const;

    // Factory: scacchiera bicolore in grayscale
    static Texture createCheckerboard(int size = 128, int cellSize = 16);

private:
    unsigned int m_id     = 0;
    int          m_width  = 0;
    int          m_height = 0;
};

} // namespace mini