#pragma once

#include <chrono>

class Timer
{
public:
    Timer(bool deferStart = false);

    void start();
    void stop();

    double elapsedMilliseconds();
    double elapsedSeconds();

private:
    std::chrono::time_point<std::chrono::system_clock> start_;
    std::chrono::time_point<std::chrono::system_clock> end_;
    bool running_ = false;
};