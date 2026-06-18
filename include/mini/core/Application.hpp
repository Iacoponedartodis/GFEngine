#pragma once
 
namespace mini
{
 
class Window; // forward declaration: Application.hpp rimane SDL2-free
 
class Application
{
public:
    Application() = default;
    ~Application() = default;
 
    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;
 
    void run();
    void requestShutdown();
 
private:
    void initialize();
    void shutdown();
    void processEvents(Window& window); // SDL_PollEvent in Application.cpp
 
    bool m_running = false;
};
 
} // namespace mini