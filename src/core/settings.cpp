#include "settings.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

void GameSettings::save(const std::string& path) const {
    std::ofstream f(path);
    if (!f) { std::cerr << "Failed to save settings to " << path << std::endl; return; }
    f << "renderDistance=" << renderDistance << "\n"
      << "brightness=" << brightness << "\n"
      << "transparentLeaves=" << (transparentLeaves ? "true" : "false") << "\n"
      << "mouseSensitivity=" << mouseSensitivity << "\n"
      << "fov=" << fov << "\n"
      << "fpsCap=" << fpsCap << "\n"
      << "vsync=" << (vsync ? "true" : "false") << "\n";
}

void GameSettings::load(const std::string& path) {
    std::ifstream f(path);
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        if (key == "renderDistance") renderDistance = std::clamp(std::stof(val), 2.0f, 32.0f);
        else if (key == "brightness") brightness = std::clamp(std::stof(val), 0.5f, 1.5f);
        else if (key == "transparentLeaves") transparentLeaves = (val == "true");
        else if (key == "mouseSensitivity") mouseSensitivity = std::clamp(std::stof(val), 0.05f, 2.0f);
        else if (key == "fov") fov = std::clamp(std::stof(val), 50.0f, 120.0f);
        else if (key == "fpsCap") fpsCap = std::clamp(std::stof(val), 10.0f, 360.0f);
        else if (key == "vsync") vsync = (val == "true");
    }
}
