#include "pch.h"

#include "Timer.h"

Timer::Timer(bool deferStart) {
    if (!deferStart) {
        start();
    }
}

void Timer::start() {
    start_ = std::chrono::system_clock::now();
    running_ = true;
}

void Timer::stop() {
    end_ = std::chrono::system_clock::now();
    running_ = false;
}

double Timer::elapsedMilliseconds() {
    std::chrono::time_point<std::chrono::system_clock> end;

    if (running_) {
        end = std::chrono::system_clock::now();
    }
    else {
        end = end_;
    }

    return double(std::chrono::duration_cast<std::chrono::milliseconds>(end - start_).count());
}

double Timer::elapsedSeconds() {
    return elapsedMilliseconds() / 1000.;
}