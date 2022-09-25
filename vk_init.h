#pragma once

#include "vk_types.h"

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_vulkan.h"
#include <vector>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan.h>

namespace vkinit {
    void setup_vulkan_instance(SDL_Window* window, ImGui_ImplVulkan_InitInfo* init_info, VkDebugReportCallbackEXT* debug_report_callback);
    void setup_vulkan_gpu(ImGui_ImplVulkan_InitInfo* init_info);
    void setup_vulkan_queue_family(ImGui_ImplVulkan_InitInfo* init_info);
    void setup_vulkan_device(ImGui_ImplVulkan_InitInfo* init_info);
    void setup_vulkan_descriptor_pool(ImGui_ImplVulkan_InitInfo* init_info);
    void setup_vulkan_swap_chain(SDL_Window* window, VkSurfaceKHR surface, ImGui_ImplVulkan_InitInfo* init_info, VkSwapchainKHR* swap_chain, std::vector<VkImage>* swap_chain_images, VkExtent2D* swap_chain_extent);
    void setup_vulkan_image_views(ImGui_ImplVulkan_InitInfo* init_info, VkSwapchainKHR swap_chain, std::vector<VkImage>* swap_chain_images, std::vector<VkImageView>* swap_chain_image_views);
    void setup_vulkan_render_pass(ImGui_ImplVulkan_InitInfo* init_info, VkRenderPass* render_pass);
    void setup_vulkan_frame_buffers(ImGui_ImplVulkan_InitInfo* init_info, std::vector<VkImageView>* swap_chain_image_views, VkExtent2D* swap_chain_extent, VkRenderPass render_pass, std::vector<VkFramebuffer>* swap_chain_frame_buffers);
    void setup_vulkan_command_pool(ImGui_ImplVulkan_InitInfo* init_info, VkCommandPool* command_pool);
    void setup_vulkan_command_buffer(ImGui_ImplVulkan_InitInfo* init_info, VkCommandPool command_pool, VkCommandBuffer* command_buffer);
    void setup_vulkan_sync_objects(ImGui_ImplVulkan_InitInfo* init_info, VkSemaphore* sem1, VkSemaphore* sem2, VkFence* fence);
    uint32_t find_memory_type(ImGui_ImplVulkan_InitInfo* init_info, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void setup_vulkan_buffer(ImGui_ImplVulkan_InitInfo* init_info, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void create_image(ImGui_ImplVulkan_InitInfo* init_info, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory, VkImageView& image_view, VkSampler& sampler, VkDescriptorSet& descriptor_set);
    void setup_emulator_texture(ImGui_ImplVulkan_InitInfo* init_info, Color display[], VkImage& textureImage, VkDeviceMemory& textureImageMemory, VkImageView& textureImageView, VkSampler& sampler, VkDescriptorSet& descriptor_set, VkBuffer& stagingBuffer, VkDeviceMemory& stagingBufferMemory);
    void copy_display_buffer(ImGui_ImplVulkan_InitInfo* init_info, Color display[], VkDeviceMemory& memory);
    void transition_image_layout(ImGui_ImplVulkan_InitInfo* init_info, VkCommandBuffer& command_buffer, VkImage& image, VkImageLayout old_layout, VkImageLayout new_layout);
    void copy_buffer_image(ImGui_ImplVulkan_InitInfo* init_info, VkCommandBuffer& command_buffer, VkBuffer& buffer, VkImage& image, uint32_t width, uint32_t height);
}