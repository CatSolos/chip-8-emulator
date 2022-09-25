#include "vk_engine.h"
#include "vk_types.h"
#include "vk_init.h"
#include "gui.h"
#include "Emulator.h"

#define SDL_MAIN_HANDLED
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_vulkan.h"
#include <stdio.h>          // printf, fprintf
#include <stdlib.h>         // abort
#include <iostream>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan.h>

//#define IMGUI_UNLIMITED_FRAME_RATE
#ifdef _DEBUG
#define IMGUI_VULKAN_DEBUG_REPORT
#endif


static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

void VulkanEngine::init() {
    // Setup SDL
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER);

    // Setup window
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    window = SDL_CreateWindow(
        "Chip 8 Interpreter",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        1280,
        720,
        window_flags
    );

    // Setup Vulkan
    vkinit::setup_vulkan_instance(window, &init_info, &debug_report_callback);
    vkinit::setup_vulkan_gpu(&init_info);
    vkinit::setup_vulkan_queue_family(&init_info);
    vkinit::setup_vulkan_device(&init_info);
    vkinit::setup_vulkan_descriptor_pool(&init_info);


    // Create Window Surface
    if (SDL_Vulkan_CreateSurface(window, init_info.Instance, &surface) == 0)
    {
        printf("Failed to create Vulkan surface.\n");
        abort();
    }

    init_info.MinImageCount = 2;
    vkinit::setup_vulkan_swap_chain(window, surface, &init_info, &swap_chain, &swap_chain_images, &swap_chain_extent);
    vkinit::setup_vulkan_image_views(&init_info, swap_chain, &swap_chain_images, &swap_chain_image_views);
    vkinit::setup_vulkan_render_pass(&init_info, &render_pass);
    vkinit::setup_vulkan_frame_buffers(&init_info, &swap_chain_image_views, &swap_chain_extent, render_pass, &swap_chain_frame_buffers);
    vkinit::setup_vulkan_command_pool(&init_info, &command_pool);
    vkinit::setup_vulkan_command_buffer(&init_info, command_pool, &command_buffer);
    vkinit::setup_vulkan_sync_objects(&init_info, &image_available_semaphore, &render_finished_semaphore, &in_flight_fence);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();
    init_info.Subpass = 0;
    ImGui_ImplSDL2_InitForVulkan(window);
    ImGui_ImplVulkan_Init(&init_info, render_pass);


    vkinit::setup_emulator_texture(&init_info, display, display_image, display_memory, display_image_view, sampler, display_descriptor_set, display_buffer, display_buffer_memory);
    vkinit::copy_display_buffer(&init_info, display, display_buffer_memory);
    vkinit::transition_image_layout(&init_info, command_buffer, display_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vkinit::copy_buffer_image(&init_info, command_buffer, display_buffer, display_image, 128, 64);
    vkinit::transition_image_layout(&init_info, command_buffer, display_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    {
        VkResult err;
        err = vkResetCommandPool(init_info.Device, command_pool, 0);
        check_vk_result(err);
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(command_buffer, &begin_info);
        check_vk_result(err);

        ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

        VkSubmitInfo end_info = {};
        end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        end_info.commandBufferCount = 1;
        end_info.pCommandBuffers = &command_buffer;
        err = vkEndCommandBuffer(command_buffer);
        check_vk_result(err);
        err = vkQueueSubmit(init_info.Queue, 1, &end_info, VK_NULL_HANDLE);
        check_vk_result(err);

        err = vkDeviceWaitIdle(init_info.Device);
        check_vk_result(err);
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    is_initialized = true;
}



void VulkanEngine::run() {
    // Main loop
    bool done = false;
    int counter = 0;
    Gui gui;
    Emulator emulator{ display };
    while (!done)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }
        // Resize swap chain?
        if (swap_chain_rebuild)
        {
            vkDeviceWaitIdle(init_info.Device);    
            for (auto frame_buffer : swap_chain_frame_buffers) {
                vkDestroyFramebuffer(init_info.Device, frame_buffer, init_info.Allocator);
            }
            for (auto image_view : swap_chain_image_views) {
                vkDestroyImageView(init_info.Device, image_view, init_info.Allocator);
            }
            vkDestroyRenderPass(init_info.Device, render_pass, init_info.Allocator);
            ImGui_ImplVulkan_SetMinImageCount(init_info.MinImageCount);
            vkinit::setup_vulkan_swap_chain(window, surface, &init_info, &swap_chain, &swap_chain_images, &swap_chain_extent);
            vkinit::setup_vulkan_render_pass(&init_info, &render_pass);
            vkinit::setup_vulkan_image_views(&init_info, swap_chain, &swap_chain_images, &swap_chain_image_views);
            vkinit::setup_vulkan_frame_buffers(&init_info, &swap_chain_image_views, &swap_chain_extent, render_pass, &swap_chain_frame_buffers);
            swap_chain_rebuild = false;
        }

        emulator.tick();
        gui.render(this, emulator);
    }
}

void VulkanEngine::cleanup() {
    VkResult err;
    err = vkDeviceWaitIdle(init_info.Device);
    check_vk_result(err);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    vkFreeMemory(init_info.Device, display_buffer_memory, init_info.Allocator);
    vkDestroyBuffer(init_info.Device, display_buffer, init_info.Allocator);

    vkDestroySampler(init_info.Device, sampler, init_info.Allocator);
    vkDestroyImageView(init_info.Device, display_image_view, init_info.Allocator);
    vkFreeMemory(init_info.Device, display_memory, init_info.Allocator);
    vkDestroyImage(init_info.Device, display_image, init_info.Allocator);

    vkDestroySemaphore(init_info.Device, image_available_semaphore, init_info.Allocator);
    vkDestroySemaphore(init_info.Device, render_finished_semaphore, init_info.Allocator);
    vkDestroyFence(init_info.Device, in_flight_fence, init_info.Allocator);
    
    vkDestroyCommandPool(init_info.Device, command_pool, init_info.Allocator);

    for (auto frame_buffer : swap_chain_frame_buffers) {
        vkDestroyFramebuffer(init_info.Device, frame_buffer, init_info.Allocator);
    }

    for (auto image_view : swap_chain_image_views) {
        vkDestroyImageView(init_info.Device, image_view, init_info.Allocator);
    }

    vkDestroyRenderPass(init_info.Device, render_pass, init_info.Allocator);
    vkDestroySwapchainKHR(init_info.Device, swap_chain, init_info.Allocator);
    vkDestroySurfaceKHR(init_info.Instance, surface, init_info.Allocator);
    vkDestroyDescriptorPool(init_info.Device, init_info.DescriptorPool, init_info.Allocator);

#ifdef IMGUI_VULKAN_DEBUG_REPORT
    // Remove the debug report callback
    auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(init_info.Instance, "vkDestroyDebugReportCallbackEXT");
    vkDestroyDebugReportCallbackEXT(init_info.Instance, debug_report_callback, init_info.Allocator);
#endif // IMGUI_VULKAN_DEBUG_REPORT

    vkDestroyDevice(init_info.Device, init_info.Allocator);
    vkDestroyInstance(init_info.Instance, init_info.Allocator);

    SDL_DestroyWindow(window);
    SDL_Quit();
}