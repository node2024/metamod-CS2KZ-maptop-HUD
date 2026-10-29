#pragma once
#include "model.h"

namespace maptop {
enum class DisplayMode { Studio, Table };
struct HudSettings {
    int menuSeconds = 120;
    DisplayMode displayMode = DisplayMode::Studio;
};
inline HudSettings ReadHudSettings(const Json& json) {
    if (!json.is_object()) throw std::runtime_error("HUD config must be an object.");
    HudSettings out;
    out.menuSeconds = std::clamp(json.value("menu_seconds", 120), 15, 600);
    const auto mode = json.value("display_mode", std::string("studio"));
    if (mode == "table") out.displayMode = DisplayMode::Table;
    else if (mode != "studio") throw std::runtime_error("display_mode must be studio or table.");
    return out;
}
} // namespace maptop
