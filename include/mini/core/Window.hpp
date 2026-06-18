#pragma once
 
#include <string>
 
// Forward declaration: mantiene SDL2 fuori dall'header pubblico.
// I dettagli SDL2 restano confinati in Window.cpp.
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
 
    // Scambia i buffer front/back (chiama SDL_GL_SwapWindow)
    void swapBuffers();
 
    // Segnala che la finestra deve chiudersi al prossimo frame
    void close();
 
    [[nodiscard]] bool isOpen()    const;
    [[nodiscard]] int  getWidth()  const;
    [[nodiscard]] int  getHeight() const;
 
private:
    SDL_Window*   m_window  = nullptr;
    SDL_GLContext m_context = nullptr;
    bool          m_open    = false;
    int           m_width   = 0;
    int           m_height  = 0;
};
 
} // namespace mini