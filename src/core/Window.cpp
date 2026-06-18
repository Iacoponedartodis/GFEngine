#include "mini/core/Window.hpp"
#include "mini/platform/OpenGL.hpp"

#include <SDL2/SDL.h>
#include <iostream>
#include <stdexcept>
#include <string>

namespace mini
{

Window::Window(const WindowConfig& config)
    : m_width(config.width)
    , m_height(config.height)
{
    SDL_SetMainReady();

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        throw std::runtime_error(
            std::string("[Window] SDL_Init fallito: ") + SDL_GetError());
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,   24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    m_window = SDL_CreateWindow(
        config.title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        config.width, config.height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!m_window)
    {
        SDL_Quit();
        throw std::runtime_error(
            std::string("[Window] SDL_CreateWindow fallito: ") + SDL_GetError());
    }

    m_context = SDL_GL_CreateContext(m_window);
    if (!m_context)
    {
        SDL_DestroyWindow(m_window);
        SDL_Quit();
        throw std::runtime_error(
            std::string("[Window] SDL_GL_CreateContext fallito: ") + SDL_GetError()
            + "\nAssicurati che la GPU supporti OpenGL 3.3 Core.");
    }

    // Carica i function pointer OpenGL 3.3 subito dopo la creazione del contesto
    if (!miniGLLoad())
    {
        SDL_GL_DeleteContext(m_context);
        SDL_DestroyWindow(m_window);
        SDL_Quit();
        throw std::runtime_error("[Window] Impossibile caricare le funzioni OpenGL 3.3.");
    }

    if (SDL_GL_SetSwapInterval(config.vsync ? 1 : 0) != 0 && config.vsync)
    {
        SDL_GL_SetSwapInterval(0);
    }

    m_open = true;

    int major = 0, minor = 0;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);

    std::cout << "[Window] Aperta " << config.width << "x" << config.height
              << " -- OpenGL " << major << "." << minor
              << " -- VSync " << (config.vsync ? "ON" : "OFF")
              << std::endl;
}

Window::~Window()
{
    if (m_context)
    {
        SDL_GL_DeleteContext(m_context);
        m_context = nullptr;
    }
    if (m_window)
    {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_Quit();
    std::cout << "[Window] Chiusa." << std::endl;
}

void Window::swapBuffers()
{
    SDL_GL_SwapWindow(m_window);
}

void Window::close()
{
    m_open = false;
}

bool Window::isOpen() const { return m_open; }
int  Window::getWidth()  const { return m_width; }
int  Window::getHeight() const { return m_height; }

} // namespace mini