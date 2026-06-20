#pragma once

namespace mini { class Window; }

namespace mini
{

class Application
{
public:
    void run();
    void requestShutdown();

private:
    void initialize();
    void shutdown();
    void processEvents(Window& window);

    bool m_running        = false;
    bool m_shootRequested = false;
};

} // namespace mini