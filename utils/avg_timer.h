#pragma once

#include <chrono>
#include <cstddef>

struct AvgTimer {
    std::chrono::steady_clock::duration total{};
    std::size_t n = 0;
    std::chrono::steady_clock::time_point start_point{};

    void start();
    void end();
    void add(std::chrono::steady_clock::duration d);
    double average_ms() const;
};