#include "latency/HighResolutionTimer.hpp"

namespace nkm
{

void HighResolutionTimer::start()
{
    m_start = Clock::now();
}

void HighResolutionTimer::stop()
{
    m_end = Clock::now();
}

double HighResolutionTimer::elapsedNanoseconds() const
{
    return std::chrono::duration<double, std::nano>(m_end - m_start).count();
}

double HighResolutionTimer::elapsedMicroseconds() const
{
    return std::chrono::duration<double, std::micro>(m_end - m_start).count();
}

double HighResolutionTimer::elapsedMilliseconds() const
{
    return std::chrono::duration<double, std::milli>(m_end - m_start).count();
}

}