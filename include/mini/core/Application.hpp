#pragma once

namespace mini
{
class Window;

class Application
{
public:
    void run(bool directPreMatch = false);

    void initialize();
    void shutdown();
    void requestShutdown();

private:
    bool m_running = false;
    bool m_shootRequested = false;
    void processEvents(Window& window);
};

} // namespace mini