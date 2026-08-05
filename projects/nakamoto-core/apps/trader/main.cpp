#include <iostream>
#include <thread>

#include "latency/HighResolutionTimer.hpp"

int main()
{
    nkm::HighResolutionTimer timer;

    timer.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    timer.stop();

    std::cout << "Elapsed: "
              << timer.elapsedMilliseconds()
              << " ms\n";

    return 0;
}