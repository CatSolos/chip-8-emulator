#include "vk_init.h"

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_vulkan.h"
#include <stdio.h>          // printf, fprintf
#include <stdlib.h>         // abort
#include <string.h>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan.h>

//#define IMGUI_UNLIMITED_FRAME_RATE
#ifdef _DEBUG
#define IMGUI_VULKAN_DEBUG_REPORT
#endif
#include <vector>

#ifdef IMGUI_VULKAN_DEBUG_REPORT
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
    (void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix; // Unused arguments
    fprintf(stderr, "[vulkan] Debug report from ObjectType: %i\nMessage: %s\n\n", objectType, pMessage);
    return VK_FALSE;
}
#endif // IMGUI_VULKAN_DEBUG_REPORT

static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

void vkinit::setup_vulkan_instance(SDL_Window* window, ImGui_ImplVulkan_InitInfo* init_info, VkDebugReportCallbackEXT* debug_report_callback) {
    VkResult err;

    uint32_t extensions_count = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &extensions_count, NULL);
    const char** extensions = new const char* [extensions_count];
    SDL_Vulkan_GetInstanceExtensions(window, &extensions_count, extensions);

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.enabledExtensionCount = extensions_count;
    create_info.ppEnabledExtensionNames = extensions;

#ifdef IMGUI_VULKAN_DEBUG_REPORT
    // Enabling validation layers
    const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
    create_info.enabledLayerCount = 1;
    create_info.ppEnabledLayerNames = layers;

    // Enable debug report extension (we need additional storage, so we duplicate the user array to add our new extension to it)
    const char** extensions_ext = (const char**)malloc(sizeof(const char*) * (extensions_count + 1));
    if (!extensions_ext) abort();
    memcpy(extensions_ext, extensions, extensions_count * sizeof(const char*));
    extensions_ext[extensions_count] = "VK_EXT_debug_report";
    create_info.enabledExtensionCount = extensions_count + 1;
    create_info.ppEnabledExtensionNames = extensions_ext;

    // Create Vulkan Instance
    err = vkCreateInstance(&create_info, init_info->Allocator, &init_info->Instance);
    check_vk_result(err);
    free(extensions_ext);

    // Get the function pointer (required for any extensions)
    auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(init_info->Instance, "vkCreateDebugReportCallbackEXT");

    // Setup the debug report callback
    VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
    debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    debug_report_ci.pfnCallback = debug_report;
    debug_report_ci.pUserData = NULL;
    err = vkCreateDebugReportCallbackEXT(init_info->Instance, &debug_report_ci, init_info->Allocator, debug_report_callback);
    check_vk_result(err);
#else
    // Create Vulkan Instance without any debug feature
    err = vkCreateInstance(&create_info, g_Allocator, &g_Instance);
    check_vk_result(err);
    IM_UNUSED(g_DebugReport);
#endif

    delete[] extensions;
}

void vkinit::setup_vulkan_gpu(ImGui_ImplVulkan_InitInfo* init_info) {
    VkResult err;

    uint32_t gpu_count;
    err = vkEnumeratePhysicalDevices(init_info->Instance, &gpu_count, NULL);
    check_vk_result(err);

    VkPhysicalDevice* gpus = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * gpu_count);
    if (!gpus) abort();

    err = vkEnumeratePhysicalDevices(init_info->Instance, &gpu_count, gpus);
    check_vk_result(err);

    // If a number >1 of GPUs got reported, find discrete GPU if present, or use first one available. This covers
    // most common cases (multi-gpu/integrated+dedicated graphics). Handling more complicated setups (multiple
    // dedicated GPUs) is out of scope of this sample.
    int use_gpu = 0;
    for (int i = 0; i < (int)gpu_count; i++)
    {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(gpus[i], &properties);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            use_gpu = i;
            break;
        }
    }

    init_info->PhysicalDevice = gpus[use_gpu];
    free(gpus);
}

void vkinit::setup_vulkan_queue_family(ImGui_ImplVulkan_InitInfo* init_info) {

    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(init_info->PhysicalDevice, &count, NULL);
    VkQueueFamilyProperties* queues = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * count);
    if (!queues) abort();
    vkGetPhysicalDeviceQueueFamilyProperties(init_info->PhysicalDevice, &count, queues);
    for (uint32_t i = 0; i < count; i++)
        if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            init_info->QueueFamily = i;
            break;
        }
    free(queues);
    if(init_info->QueueFamily == (uint32_t)-1) abort();
}

void vkinit::setup_vulkan_device(ImGui_ImplVulkan_InitInfo* init_info) {
    VkResult err;

    int device_extension_count = 1;
    const char* device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    const float queue_priority[] = { 1.0f };
    VkDeviceQueueCreateInfo queue_info[1] = {};
    queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info[0].queueFamilyIndex = init_info->QueueFamily;
    queue_info[0].queueCount = 1;
    queue_info[0].pQueuePriorities = queue_priority;
    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
    create_info.pQueueCreateInfos = queue_info;
    create_info.enabledExtensionCount = device_extension_count;
    create_info.ppEnabledExtensionNames = device_extensions;
    err = vkCreateDevice(init_info->PhysicalDevice, &create_info, init_info->Allocator, &init_info->Device);
    check_vk_result(err);
    vkGetDeviceQueue(init_info->Device, init_info->QueueFamily, 0, &init_info->Queue);
}

void vkinit::setup_vulkan_descriptor_pool(ImGui_ImplVulkan_InitInfo* init_info) {
    VkResult err;

    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    err = vkCreateDescriptorPool(init_info->Device, &pool_info, init_info->Allocator, &init_info->DescriptorPool);
    check_vk_result(err);
}

void vkinit::setup_vulkan_swap_chain(SDL_Window* window, VkSurfaceKHR surface, ImGui_ImplVulkan_InitInfo* init_info, VkSwapchainKHR* swap_chain, std::vector<VkImage>* swap_chain_images, VkExtent2D* swap_chain_extent) {
    VkSurfaceCapabilitiesKHR capabilities = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(init_info->PhysicalDevice, surface, &capabilities);

    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    swap_chain_extent->width = width;
    swap_chain_extent->height = height;

    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;
    create_info.minImageCount = init_info->MinImageCount;
    create_info.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
    create_info.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    create_info.imageExtent = *swap_chain_extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.preTransform = capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = *swap_chain;
    VkResult err;
    err = vkDeviceWaitIdle(init_info->Device);
    check_vk_result(err);
    err = vkCreateSwapchainKHR(init_info->Device, &create_info, init_info->Allocator, swap_chain);
    check_vk_result(err);
    if (create_info.oldSwapchain)
        vkDestroySwapchainKHR(init_info->Device, create_info.oldSwapchain, init_info->Allocator);
    uint32_t image_count;
    vkGetSwapchainImagesKHR(init_info->Device, *swap_chain, &image_count, nullptr);
    swap_chain_images->resize(image_count);
    vkGetSwapchainImagesKHR(init_info->Device, *swap_chain, &image_count, swap_chain_images->data());
    init_info->ImageCount = image_count;
}

void vkinit::setup_vulkan_image_views(ImGui_ImplVulkan_InitInfo* init_info, VkSwapchainKHR swap_chain, std::vector<VkImage>* swap_chain_images, std::vector<VkImageView>* swap_chain_image_views) {
    swap_chain_image_views->resize(swap_chain_images->size());
    for (size_t i = 0; i < swap_chain_images->size(); i++) {
        VkImageViewCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = swap_chain_images->at(i);
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = VK_FORMAT_B8G8R8A8_SRGB;
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;
        VkResult err;
        err = vkCreateImageView(init_info->Device, &create_info, init_info->Allocator, &swap_chain_image_views->at(i));
        check_vk_result(err);
    }
}

void vkinit::setup_vulkan_render_pass(ImGui_ImplVulkan_InitInfo* init_info, VkRenderPass* render_pass) {
    init_info->MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = VK_FORMAT_B8G8R8A8_SRGB;
    color_attachment.samples = init_info->MSAASamples;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.attachmentCount = 1;
    create_info.pAttachments = &color_attachment;
    create_info.subpassCount = 1;
    create_info.pSubpasses = &subpass;
    create_info.dependencyCount = 1;
    create_info.pDependencies = &dependency;

    VkResult err;
    err = vkCreateRenderPass(init_info->Device, &create_info, init_info->Allocator, render_pass);
    check_vk_result(err);
}

void vkinit::setup_vulkan_frame_buffers(ImGui_ImplVulkan_InitInfo* init_info, std::vector<VkImageView>* swap_chain_image_views, VkExtent2D* swap_chain_extent, VkRenderPass render_pass, std::vector<VkFramebuffer>* swap_chain_frame_buffers) {
    swap_chain_frame_buffers->resize(swap_chain_image_views->size());
    for (size_t i = 0; i < swap_chain_image_views->size(); i++) {
        VkImageView attachments[] = {
            swap_chain_image_views->at(i)
        };

        VkFramebufferCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        create_info.renderPass = render_pass;
        create_info.attachmentCount = 1;
        create_info.pAttachments = attachments;
        create_info.width = swap_chain_extent->width;
        create_info.height = swap_chain_extent->height;
        create_info.layers = 1;

        VkResult err;
        err = vkCreateFramebuffer(init_info->Device, &create_info, init_info->Allocator, &swap_chain_frame_buffers->at(i));
        check_vk_result(err);
    }
}

void vkinit::setup_vulkan_command_pool(ImGui_ImplVulkan_InitInfo* init_info, VkCommandPool* command_pool) {
    VkCommandPoolCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    create_info.queueFamilyIndex = init_info->QueueFamily;
    VkResult err;
    err = vkCreateCommandPool(init_info->Device, &create_info, init_info->Allocator, command_pool);
    check_vk_result(err);
}

void vkinit::setup_vulkan_command_buffer(ImGui_ImplVulkan_InitInfo* init_info, VkCommandPool command_pool, VkCommandBuffer* command_buffer) {
    VkCommandBufferAllocateInfo allocate_info = {};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = command_pool;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = 1;
    VkResult err;
    err = vkAllocateCommandBuffers(init_info->Device, &allocate_info, command_buffer);
    check_vk_result(err);
}

void vkinit::setup_vulkan_sync_objects(ImGui_ImplVulkan_InitInfo* init_info, VkSemaphore* sem1, VkSemaphore* sem2, VkFence* fence) {
    VkSemaphoreCreateInfo sem_info = {};
    sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkResult err;
    err = vkCreateSemaphore(init_info->Device, &sem_info, init_info->Allocator, sem1);
    check_vk_result(err);
    err = vkCreateSemaphore(init_info->Device, &sem_info, init_info->Allocator, sem2);
    check_vk_result(err);
    err = vkCreateFence(init_info->Device, &fence_info, init_info->Allocator, fence);
    check_vk_result(err);
}

uint32_t vkinit::find_memory_type(ImGui_ImplVulkan_InitInfo* init_info, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(init_info->PhysicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    abort();
}

void vkinit::setup_vulkan_buffer(ImGui_ImplVulkan_InitInfo* init_info, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult err;
    err = vkCreateBuffer(init_info->Device, &bufferInfo, nullptr, &buffer);
    check_vk_result(err);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(init_info->Device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vkinit::find_memory_type(init_info, memRequirements.memoryTypeBits, properties);

    err = vkAllocateMemory(init_info->Device, &allocInfo, nullptr, &bufferMemory);
    check_vk_result(err);

    vkBindBufferMemory(init_info->Device, buffer, bufferMemory, 0);
}

void vkinit::create_image(ImGui_ImplVulkan_InitInfo* init_info, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory, VkImageView& image_view, VkSampler& sampler, VkDescriptorSet& descriptor_set) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult err;
    err = vkCreateImage(init_info->Device, &imageInfo, nullptr, &image);
    check_vk_result(err);

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(init_info->Device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vkinit::find_memory_type(init_info, memRequirements.memoryTypeBits, properties);

    err = vkAllocateMemory(init_info->Device, &allocInfo, nullptr, &imageMemory);
    check_vk_result(err);

    vkBindImageMemory(init_info->Device, image, imageMemory, 0);

    VkImageViewCreateInfo image_view_info{};
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.image = image;
    image_view_info.format = format;
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.layerCount = 1;
    image_view_info.subresourceRange.levelCount = 1;
    vkCreateImageView(init_info->Device, &image_view_info, init_info->Allocator, &image_view);

    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_NEAREST;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.minLod = -1000;
    sampler_info.maxLod = VK_LOD_CLAMP_NONE;
    vkCreateSampler(init_info->Device, &sampler_info, init_info->Allocator, &sampler);

    descriptor_set = ImGui_ImplVulkan_AddTexture(sampler, image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void vkinit::setup_emulator_texture(ImGui_ImplVulkan_InitInfo* init_info, Color display[], VkImage& textureImage, VkDeviceMemory& textureImageMemory, VkImageView& textureImageView, VkSampler& sampler, VkDescriptorSet& descriptor_set, VkBuffer& stagingBuffer, VkDeviceMemory& stagingBufferMemory) {
    VkDeviceSize imageSize = 128 * 64 * sizeof(Color);

    vkinit::setup_vulkan_buffer(init_info, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    vkinit::create_image(init_info, 128, 64, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory, textureImageView, sampler, descriptor_set);
}

void vkinit::copy_display_buffer(ImGui_ImplVulkan_InitInfo* init_info, Color display[], VkDeviceMemory& memory) {
    VkDeviceSize imageSize = 128 * 64 * sizeof(Color);
    void* data;
    vkMapMemory(init_info->Device, memory, 0, imageSize, 0, &data);
    memcpy(data, display, static_cast<size_t>(imageSize));
    vkUnmapMemory(init_info->Device, memory);
}

void vkinit::transition_image_layout(ImGui_ImplVulkan_InitInfo* init_info, VkCommandBuffer& command_buffer, VkImage& image, VkImageLayout old_layout, VkImageLayout new_layout) {
    VkResult err;
    err = vkResetCommandBuffer(command_buffer, 0);
    check_vk_result(err);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(command_buffer, &begin_info);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    vkQueueSubmit(init_info->Queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(init_info->Queue);
}

void vkinit::copy_buffer_image(ImGui_ImplVulkan_InitInfo* init_info, VkCommandBuffer& command_buffer, VkBuffer& buffer, VkImage& image, uint32_t width, uint32_t height) {
    VkResult err;
    err = vkResetCommandBuffer(command_buffer, 0);
    check_vk_result(err);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(command_buffer, &begin_info);

    VkBufferImageCopy copy_info{};
    copy_info.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_info.imageSubresource.layerCount = 1;
    copy_info.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(command_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_info);

    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    vkQueueSubmit(init_info->Queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(init_info->Queue);
}