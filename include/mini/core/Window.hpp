#pragma once

#include <string>

struct SDL_Window;
using  SDL_GLContext = void*;

namespace mini
{

struct WindowConfig
{
    std::string title  = "GFEngine";
    int         width  = 1280;
    int         height = 720;
    bool        vsync  = true;
};

class Window
{
public:
    explicit Window(const WindowConfig& config = {});
    ~Window();

    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;

    void swapBuffers();
    void close();

    // Cattura il mouse per la modalita' FPS (cursore nascosto, movimento relativo).
    // Tab per alternare durante il gioco.
    void setMouseCaptured(bool captured);

    [[nodiscard]] bool isOpen()          const;
    [[nodiscard]] bool isMouseCaptured() const;
    [[nodiscard]] int  getWidth()        const;
    [[nodiscard]] int  getHeight()       const;

private:
    SDL_Window*   m_window        = nullptr;
    SDL_GLContext m_context       = nullptr;
    bool          m_open          = false;
    bool          m_mouseCaptured = false;
    int           m_width         = 0;
    int           m_height        = 0;
};

} // namespace mini