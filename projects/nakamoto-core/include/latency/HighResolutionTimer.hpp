#pragma once

#include <chrono>

namespace nkm
{

class HighResolutionTimer
{
public:

    void start();

    void stop();

    double elapsedNanoseconds() const;

    double elapsedMicroseconds() const;

    double elapsedMilliseconds() const;

private:

    using Clock = std::chrono::high_resolution_clock;

    Clock::time_point m_start;

    Clock::time_point m_end;
};

}