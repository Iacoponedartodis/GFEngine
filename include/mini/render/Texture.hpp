#pragma once

#include <optional>
#include <string>

namespace mini
{

class Texture
{
public:
    Texture(const unsigned char* data, int width, int height, int channels = 3);
    ~Texture();

    Texture(const Texture&)            = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    void bind(unsigned int slot = 0) const;
    void unbind() const;

    [[nodiscard]] unsigned int getId()     const;
    [[nodiscard]] int          getWidth()  const;
    [[nodiscard]] int          getHeight() const;
    [[nodiscard]] const std::string& getPath() const;

    // Carica PNG/JPG/BMP/TGA da file con stb_image.
    // flipY=true: capovolge verticalmente (necessario per OpenGL).
    // Ritorna nullopt se il file non esiste o non e' leggibile.
    static std::optional<Texture> loadFromFile(const char* path, bool flipY = true);

    // Genera una texture procedurale a scacchiera (fallback quando il file manca)
    static Texture createCheckerboard(int size = 128, int cellSize = 16);

private:
    unsigned int m_id     = 0;
    int          m_width  = 0;
    int          m_height = 0;
    std::string  m_path;   // path originale per debug
};

} // namespace mini