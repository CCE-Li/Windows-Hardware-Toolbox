#pragma once

#include <deque>
#include <string>

#include "imgui.h"

namespace htb {

class HistoryChart {
public:
    HistoryChart(size_t capacity = 300, double windowSeconds = 90.0);

    void sample(double value, double nowSeconds);
    void clear();

    void draw(const char* label, float height, float yMax, bool percentMode,
              const std::string& valueText, const std::string& peakText);

    double maxInWindow() const;
    double lastValue() const;
    size_t pointCount() const { return m_points.size(); }

private:
    std::deque<std::pair<double, double>> m_points;
    size_t m_capacity;
    double m_windowSeconds;
    double m_lastSampleTime = -1.0;
};

} // namespace htb
