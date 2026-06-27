#include "avg_timer.h"

void AvgTimer::start() {
    start_point = std::chrono::steady_clock::now();
}

void AvgTimer::end() {
    auto end_point = std::chrono::steady_clock::now();
    add(end_point - start_point);
    start_point = std::chrono::steady_clock::time_point{};
}

void AvgTimer::add(std::chrono::steady_clock::duration d) {
    total += d;
    ++n;
}

double AvgTimer::average_ms() const {
    return n
        ? std::chrono::duration<double, std::milli>(total).count() / n
        : 0.0;
}

double AvgTimer::total_ms() const {
    return std::chrono::duration<double, std::milli>(total).count();
}