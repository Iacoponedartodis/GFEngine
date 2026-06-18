#include "mini/core/Clock.hpp"
 
namespace mini
{
 
Clock::Clock()
    : m_start(std::chrono::steady_clock::now())
{
}
 
float Clock::restart()
{
    const TimePoint now     = std::chrono::steady_clock::now();
    const float     elapsed = std::chrono::duration<float>(now - m_start).count();
    m_start = now;
    return elapsed;
}
 
float Clock::getElapsed() const
{
    const TimePoint now = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(now - m_start).count();
}
 
} // namespace mini