#pragma once
#include "vk_engine.h"
#include "Emulator.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include <vulkan.h>
class Gui
{
private:
    bool show_demo_window{ false };
    bool show_emu_window{ true };
    struct ImVec4 clear_color{ 0.45f, 0.55f, 0.60f, 1.00f };

    void draw(ImDrawData* draw_data, VulkanEngine* engine, uint32_t index);
    void present(VulkanEngine* engine, uint32_t index);
public:
    void render(VulkanEngine* engine, Emulator& emulator);
};