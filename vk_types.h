#pragma once
#include "imgui.h"
#include <vulkan.h>

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
    bool operator==(const Color& rhs) {
        return (r == rhs.r && g == rhs.g && b == rhs.b && a == rhs.a);
    }
    Color operator^(const Color& rhs) {
        return Color{ (uint8_t)(r ^ rhs.r), (uint8_t)(g ^ rhs.g), (uint8_t)(b ^ rhs.b), (uint8_t)(a ^ rhs.a) };
    }
    Color operator=(const Color& rhs) {
        r = rhs.r;
        g = rhs.g;
        b = rhs.b;
        a = rhs.a;
        return *this;
    }
    Color operator=(const ImVec4& rhs) {
        r = (uint8_t)(rhs.x * 255);
        g = (uint8_t)(rhs.y * 255);
        b = (uint8_t)(rhs.z * 255);
        a = (uint8_t)(rhs.w * 255);
        return *this;
    }
    Color operator^=(const Color& rhs) {
        *this = *this ^ rhs;
        return *this;
    }
};