#pragma once

#include "vk_types.h"

#include "imgui_impl_vulkan.h"
#include <vector>

class VulkanEngine
{
private:
    void draw(struct ImDrawData* draw_data);

    void present();

public:
    bool                            is_initialized{ false };
    int                             frame_number{ 0 };
    struct SDL_Window* window{ nullptr };
    VkDebugReportCallbackEXT        debug_report_callback{ nullptr };
    ImGui_ImplVulkan_InitInfo       init_info{};
    VkSurfaceKHR                    surface{ nullptr };
    VkSwapchainKHR                  swap_chain{ nullptr };
    VkExtent2D                      swap_chain_extent{};
    std::vector<VkImage>            swap_chain_images;
    std::vector<VkImageView>        swap_chain_image_views;
    VkRenderPass                    render_pass{ nullptr };
    std::vector<VkFramebuffer>      swap_chain_frame_buffers;
    VkCommandPool                   command_pool{ nullptr };
    VkCommandBuffer                 command_buffer{ nullptr };
    VkSemaphore                     image_available_semaphore;
    VkSemaphore                     render_finished_semaphore;
    VkFence                         in_flight_fence;
    Color                           display[128 * 64];
    VkBuffer                        display_buffer{ nullptr };
    VkDeviceMemory                  display_buffer_memory{ nullptr };
    VkImage                         display_image{ nullptr };
    VkImageView                     display_image_view{ nullptr };
    VkDeviceMemory                  display_memory{ nullptr };
    VkSampler                       sampler{ nullptr };
    VkDescriptorSet                 display_descriptor_set{ nullptr };

    bool                            swap_chain_rebuild{ false };

    void init();

    void cleanup();

    void run();
};