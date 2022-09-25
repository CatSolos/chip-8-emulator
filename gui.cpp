#include "gui.h"
#include "vk_engine.h"
#include "vk_init.h"

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_vulkan.h"
#include <stdio.h>          // printf, fprintf
#include <stdlib.h>         // abort
#include <vector>
#include <vulkan.h>
#include "Emulator.h"

static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

void Gui::draw(ImDrawData* draw_data, VulkanEngine* engine, uint32_t index) {

    VkResult err;

    //VkSemaphore image_acquired_semaphore = window_data->FrameSemaphores[window_data->SemaphoreIndex].ImageAcquiredSemaphore;
    //VkSemaphore render_complete_semaphore = window_data->FrameSemaphores[window_data->SemaphoreIndex].RenderCompleteSemaphore;

    //ImGui_ImplVulkanH_Frame* fd = &window_data->Frames[window_data->FrameIndex];
    {
        err = vkWaitForFences(engine->init_info.Device, 1, &engine->in_flight_fence, VK_TRUE, UINT64_MAX);    // wait indefinitely instead of periodically checking
        check_vk_result(err);

        err = vkResetFences(engine->init_info.Device, 1, &engine->in_flight_fence);
       check_vk_result(err);
    }
    {
        vkinit::copy_display_buffer(&engine->init_info, engine->display, engine->display_buffer_memory);
        vkinit::transition_image_layout(&engine->init_info, engine->command_buffer, engine->display_image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        vkinit::copy_buffer_image(&engine->init_info, engine->command_buffer, engine->display_buffer, engine->display_image, 128, 64);
        vkinit::transition_image_layout(&engine->init_info, engine->command_buffer, engine->display_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    {
        err = vkResetCommandBuffer(engine->command_buffer, 0);
        check_vk_result(err);
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        //info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        info.flags = 0;
        err = vkBeginCommandBuffer(engine->command_buffer, &info);
        check_vk_result(err);
    }
    {
        VkClearValue clearColor = { {{0.3f, 0.3f, 1.0f, 1.0f}} };
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = engine->render_pass;
        info.framebuffer = engine->swap_chain_frame_buffers[index];
        info.renderArea.extent.width = engine->swap_chain_extent.width;
        info.renderArea.extent.height = engine->swap_chain_extent.height;
        info.clearValueCount = 1;
        info.pClearValues = &clearColor;
        vkCmdBeginRenderPass(engine->command_buffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    // Record dear imgui primitives into command buffer
    ImGui_ImplVulkan_RenderDrawData(draw_data, engine->command_buffer);

    // Submit command buffer
    vkCmdEndRenderPass(engine->command_buffer);
    {
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &engine->image_available_semaphore;
        info.pWaitDstStageMask = &wait_stage;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &engine->command_buffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &engine->render_finished_semaphore;

        err = vkEndCommandBuffer(engine->command_buffer);
        check_vk_result(err);
        err = vkQueueSubmit(engine->init_info.Queue, 1, &info, engine->in_flight_fence);
        check_vk_result(err);
    }
}

void Gui::present(VulkanEngine* engine, uint32_t index) {
    if (engine->swap_chain_rebuild)
        return;
    //VkSemaphore render_complete_semaphore = window_data->FrameSemaphores[window_data->SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &engine->render_finished_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &engine->swap_chain;
    info.pImageIndices = &index;
    VkResult err = vkQueuePresentKHR(engine->init_info.Queue, &info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
    {
        engine->swap_chain_rebuild = true;
        return;
    }
    check_vk_result(err);
    //window_data->SemaphoreIndex = (window_data->SemaphoreIndex + 1) % window_data->ImageCount; // Now we can use the next set of semaphores
}

void Gui::render(VulkanEngine* engine, Emulator& emulator) {
    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);


    if (show_emu_window)
    {
        ImGui::Begin("Interpreter Window", &show_emu_window);
        ImGui::Image((ImTextureID)engine->display_descriptor_set, { ImGui::GetWindowWidth()-15, ImGui::GetWindowHeight() - 35 });
        ImGui::End();
    }

    emulator.render();

    // Rendering
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
    if (!is_minimized)
    {
        VkResult err;
        uint32_t index;
        err = vkAcquireNextImageKHR(engine->init_info.Device, engine->swap_chain, UINT64_MAX, engine->image_available_semaphore, VK_NULL_HANDLE, &index);
        if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
        {
            engine->swap_chain_rebuild = true;
            return;
        }
        check_vk_result(err);
        draw(draw_data, engine, index);
        present(engine, index);
    }
}