#pragma once

#include <string>
#include <unordered_map>

#include "ui/pages/Page.h"
#include "ui/widgets/HistoryChart.h"

namespace htb {

class SensorsPage final : public IPage {
public:
    std::string_view title() const override { return "传感器"; }
    void draw(UiContext& ctx) override;

private:
    std::unordered_map<std::string, HistoryChart> m_charts;
};

} // namespace htb