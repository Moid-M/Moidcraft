#pragma once
#include <string>

#include <algorithm>
#include <cmath>

struct GameSettings {
    float renderDistance = 8.0f;   // 2-32 (stored as float for slider)
    float brightness = 1.0f;       // 0.5=moody, 1.0=normal, 1.5=bright
    bool transparentLeaves = false;
    float mouseSensitivity = 0.1f; // 0.05-2.0
    float fov = 75.0f;             // 50-120
    float fpsCap = 120.0f;         // 10-360
    bool vsync = true;

    int renderDistanceInt() const { return (int)std::round(renderDistance); }
    int fovInt() const { return (int)std::round(fov); }
    int fpsCapInt() const { return (int)std::round(fpsCap); }

    void save(const std::string& path) const;
    void load(const std::string& path);
};
