#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <U_Colors.h>
#include <M_Log.h>
#include <M_Window.h>
#include <R_Device.h>

// ============================================================================
// Modern Vulkan 1.3+ Swapchain & Frame Presenter
// ============================================================================
// Features:
//   - Native High-DPI Resolution via Window::Get_PixelSize()
//   - Modern Vulkan 1.3 Dynamic Rendering (vkCmdBeginRendering with clear color)
//   - Modern Synchronization 2 (vkCmdPipelineBarrier2 & vkQueueSubmit2)
//   - Triple Buffering + VSync Support (MAILBOX / FIFO)
//   - Automatic Resizing & Minimize Handling (No freeze, crash, or memory leak)
// ============================================================================
class RenderSwapchain {
public:
    static constexpr UINT MAX_FRAMES_IN_FLIGHT = 3; // Triple-buffering CPU/GPU pipeline

private:
    static inline VkSwapchainKHR       s_Swapchain         = VK_NULL_HANDLE;
    static inline VkFormat             s_ImageFormat       = VK_FORMAT_B8G8R8A8_SRGB;
    static inline VkColorSpaceKHR      s_ColorSpace        = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    static inline VkExtent2D           s_Extent            = { 1280, 720 };
    static inline Array<VkImage>       s_Images;
    static inline Array<VkImageView>   s_ImageViews;

    // Per-frame CPU-GPU sync primitives
    static inline fArray<VkSemaphore, MAX_FRAMES_IN_FLIGHT> s_ImageAvailableSemaphores{};
    static inline Array<VkSemaphore>                        s_RenderFinishedSemaphores;
    static inline fArray<VkFence, MAX_FRAMES_IN_FLIGHT>     s_InFlightFences{};
    static inline fArray<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> s_CommandBuffers{};

    static inline UINT  s_CurrentFrameIndex = 0;
    static inline UINT  s_CurrentImageIndex = 0;
    static inline BOOL  s_IsInitialized     = false;
    static inline BOOL  s_NeedsResize       = false;
    static inline BOOL  s_VSync             = true;
    static inline COLOR s_ClearColor        = COLOR(30, 40, 55, 255); // Dark Slate Blue

    // Pick best format (B8G8R8A8 SRGB is the industry standard)
    static inline VkSurfaceFormatKHR ChooseSurfaceFormat(const Array<VkSurfaceFormatKHR> &availableFormats) {
        for (const auto &format : availableFormats) {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
        }
        return availableFormats[0];
    }

    static inline const Char *PresentModeToString(VkPresentModeKHR mode) {
        switch (mode) {
            case VK_PRESENT_MODE_IMMEDIATE_KHR: return "IMMEDIATE (Uncapped)";
            case VK_PRESENT_MODE_MAILBOX_KHR:   return "MAILBOX (Fast Triple-Buffer)";
            case VK_PRESENT_MODE_FIFO_KHR:      return "FIFO (VSync)";
            default:                            return "Other";
        }
    }

    // Pick best present mode: IMMEDIATE for uncapped FPS, or FIFO for strict VSync
    static inline VkPresentModeKHR ChoosePresentMode(const Array<VkPresentModeKHR> &availableModes, BOOL vSync) {
        if (!vSync) {
            // Immediate = True uncapped FPS (renders as fast as GPU can go without waiting for monitor V-Blank)
            for (const auto &mode : availableModes) {
                if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) return mode;
            }
            for (const auto &mode : availableModes) {
                if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR; // Strict VSync (matches monitor refresh rate: 60Hz, 144Hz, 165Hz)
    }

    // Match exact pixel size from the OS Window
    static inline VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR &capabilities) {
        if (capabilities.currentExtent.width != UINT32_MAX) {
            return capabilities.currentExtent;
        }

        Vector2D pixelSize = Window::Get_PixelSize();
        VkExtent2D actualExtent = {
            static_cast<UINT>(pixelSize.x),
            static_cast<UINT>(pixelSize.y)
        };

        actualExtent.width  = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }

    static inline BOOL CreateInternal() {
        VkDevice device           = R_Device::Get_Device();
        VkPhysicalDevice physDev  = R_Device::Get_PhysicalDevice();
        VkSurfaceKHR surface      = R_Device::Get_Surface();

        // 1. Query Surface Capabilities
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDev, surface, &capabilities);

        UINT formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physDev, surface, &formatCount, nullptr);
        Array<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physDev, surface, &formatCount, formats.data());

        UINT presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physDev, surface, &presentModeCount, nullptr);
        Array<VkPresentModeKHR> presentModes(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physDev, surface, &presentModeCount, presentModes.data());

        VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(formats);
        VkPresentModeKHR   presentMode   = ChoosePresentMode(presentModes, s_VSync);
        s_Extent                         = ChooseExtent(capabilities);
        s_ImageFormat                    = surfaceFormat.format;
        s_ColorSpace                     = surfaceFormat.colorSpace;

        // Quad buffering when uncapped so DWM never starves the acquire queue
        UINT imageCount = s_VSync ? (capabilities.minImageCount + 1) : (capabilities.minImageCount + 2);
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
            imageCount = capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface          = surface;
        createInfo.minImageCount    = imageCount;
        createInfo.imageFormat      = s_ImageFormat;
        createInfo.imageColorSpace  = s_ColorSpace;
        createInfo.imageExtent      = s_Extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        UINT queueFamilyIndices[] = { R_Device::Get_GraphicsQueueFamily(), R_Device::Get_PresentQueueFamily() };
        if (queueFamilyIndices[0] != queueFamilyIndices[1]) {
            createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices   = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
        }

        createInfo.preTransform   = capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode    = presentMode;
        createInfo.clipped        = VK_TRUE;
        createInfo.oldSwapchain   = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &s_Swapchain) != VK_SUCCESS) {
            LOG(Error, LogLevel::Core, "RenderSwapchain failed to create VkSwapchainKHR!");
            return false;
        }

        // 2. Retrieve swapchain images
        UINT actualImageCount = 0;
        vkGetSwapchainImagesKHR(device, s_Swapchain, &actualImageCount, nullptr);
        s_Images.resize(actualImageCount);
        vkGetSwapchainImagesKHR(device, s_Swapchain, &actualImageCount, s_Images.data());

        // 3. Create Image Views
        s_ImageViews.resize(actualImageCount);
        for (size_t i = 0; i < actualImageCount; ++i) {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image                           = s_Images[i];
            viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format                          = s_ImageFormat;
            viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel   = 0;
            viewInfo.subresourceRange.levelCount     = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount     = 1;

            if (vkCreateImageView(device, &viewInfo, nullptr, &s_ImageViews[i]) != VK_SUCCESS) {
                LOG(Error, LogLevel::Core, "RenderSwapchain failed to create image view [{}]!", i);
                return false;
            }
        }

        // 4. Create one RenderFinished semaphore per swapchain image
        s_RenderFinishedSemaphores.resize(actualImageCount);
        VkSemaphoreCreateInfo semInfo{};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        for (size_t i = 0; i < actualImageCount; ++i) {
            if (vkCreateSemaphore(device, &semInfo, nullptr, &s_RenderFinishedSemaphores[i]) != VK_SUCCESS) {
                LOG(Error, LogLevel::Core, "RenderSwapchain failed to create RenderFinished semaphore [{}]!", i);
                return false;
            }
        }

        LOG(Info, LogLevel::Core, "RenderSwapchain created ({}x{}, {} images, VSync: {}, Mode: {})",
            s_Extent.width, s_Extent.height, actualImageCount, s_VSync ? "ON" : "OFF", PresentModeToString(presentMode));
        return true;
    }

    static inline void CleanupSwapchain() {
        VkDevice device = R_Device::Get_Device();
        if (!device) return;

        for (auto sem : s_RenderFinishedSemaphores) {
            if (sem) vkDestroySemaphore(device, sem, nullptr);
        }
        s_RenderFinishedSemaphores.clear();

        for (auto imageView : s_ImageViews) {
            if (imageView) vkDestroyImageView(device, imageView, nullptr);
        }
        s_ImageViews.clear();
        s_Images.clear();

        if (s_Swapchain) {
            vkDestroySwapchainKHR(device, s_Swapchain, nullptr);
            s_Swapchain = VK_NULL_HANDLE;
        }
    }

public:
    // ========================================================================
    // Init & Shutdown
    // ========================================================================

    // Initializes the swapchain, image views, command buffers and sync fences.
    // Reads VSync from Config [Window] (default: true).
    // Example: R_Swapchain::Init();
    static inline BOOL Init(BOOL defaultVSync = true) {
        if (s_IsInitialized) return true;
        s_VSync = Config::Get("Window", "VSync", defaultVSync);

        VkDevice device = R_Device::Get_Device();
        if (!device) {
            LOG(Error, LogLevel::Core, "RenderSwapchain::Init failed: RenderDevice is not initialized!");
            return false;
        }

        if (!CreateInternal()) return false;

        // Allocate per-frame Command Buffers from R_Device's Command Pool
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = R_Device::Get_CommandPool();
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

        if (vkAllocateCommandBuffers(device, &allocInfo, s_CommandBuffers.data()) != VK_SUCCESS) {
            LOG(Error, LogLevel::Core, "RenderSwapchain::Init failed to allocate command buffers!");
            return false;
        }

        // Create per-frame Synchronization Primitives (Semaphores & Fences)
        VkSemaphoreCreateInfo semInfo{};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled so frame 0 doesn't wait!

        for (UINT i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (vkCreateSemaphore(device, &semInfo, nullptr, &s_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device, &fenceInfo, nullptr, &s_InFlightFences[i]) != VK_SUCCESS)
            {
                LOG(Error, LogLevel::Core, "RenderSwapchain::Init failed to create sync objects for frame [{}]!", i);
                return false;
            }
        }

        s_IsInitialized = true;
        return true;
    }

    // Handles window resizing, minimizing and fullscreen toggling smoothly.
    static inline void Recreate() {
        Vector2D px = Window::Get_PixelSize();
        if (px.x <= 0 || px.y <= 0) return; // Minimized to taskbar, skip

        VkDevice device = R_Device::Get_Device();
        if (!device) return;

        vkDeviceWaitIdle(device);
        CleanupSwapchain();
        CreateInternal();
        s_NeedsResize = false;
    }

    // Shuts down swapchain and releases all GPU resources.
    static inline void Shutdown() {
        if (!s_IsInitialized) return;

        VkDevice device = R_Device::Get_Device();
        if (device) {
            vkDeviceWaitIdle(device);

            for (UINT i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                if (s_ImageAvailableSemaphores[i]) {
                    vkDestroySemaphore(device, s_ImageAvailableSemaphores[i], nullptr);
                    s_ImageAvailableSemaphores[i] = VK_NULL_HANDLE;
                }
                if (s_InFlightFences[i]) {
                    vkDestroyFence(device, s_InFlightFences[i], nullptr);
                    s_InFlightFences[i] = VK_NULL_HANDLE;
                }
            }

            if (R_Device::Get_CommandPool()) {
                vkFreeCommandBuffers(device, R_Device::Get_CommandPool(), MAX_FRAMES_IN_FLIGHT, s_CommandBuffers.data());
            }

            CleanupSwapchain();
        }

        s_IsInitialized = false;
        LOG(Info, LogLevel::Core, "RenderSwapchain::Shutdown cleaned up swapchain.");
    }

    // ========================================================================
    // Rendering Loop (BeginFrame -> Draw -> EndFrame)
    // ========================================================================

    // Prepares the screen for rendering. Clears to s_ClearColor using Modern Dynamic Rendering.
    // Returns TRUE if the frame is ready for drawing; FALSE if minimized or resizing.
    // Example: if (R_Swapchain::BeginFrame()) { ... R_Swapchain::EndFrame(); }
    static inline BOOL BeginFrame() {
        if (!s_IsInitialized) return false;
        if (Window::Is_Minimized()) return false;

        if (s_NeedsResize || Window::Was_Resized()) {
            Recreate();
            return false;
        }

        VkDevice device = R_Device::Get_Device();

        // 1. Wait for GPU to finish previous frame on this slot
        vkWaitForFences(device, 1, &s_InFlightFences[s_CurrentFrameIndex], VK_TRUE, UINT64_MAX);

        // 2. Acquire next swapchain image
        VkResult acquireResult = vkAcquireNextImageKHR(
            device,
            s_Swapchain,
            UINT64_MAX,
            s_ImageAvailableSemaphores[s_CurrentFrameIndex],
            VK_NULL_HANDLE,
            &s_CurrentImageIndex
        );

        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
            Recreate();
            return false;
        } else if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
            LOG(Error, LogLevel::Core, "RenderSwapchain failed to acquire swapchain image!");
            return false;
        }

        vkResetFences(device, 1, &s_InFlightFences[s_CurrentFrameIndex]);

        // 3. Begin Recording Commands
        VkCommandBuffer cmd = s_CommandBuffers[s_CurrentFrameIndex];
        vkResetCommandBuffer(cmd, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(cmd, &beginInfo);

        // 4. Modern Vulkan 1.3 Transition: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL via Synchronization2
        VkImageMemoryBarrier2 barrier{};
        barrier.sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.srcAccessMask = 0;
        barrier.dstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.image         = s_Images[s_CurrentImageIndex];
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;

        VkDependencyInfo depInfo{};
        depInfo.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers    = &barrier;
        vkCmdPipelineBarrier2(cmd, &depInfo);

        // 5. Modern Vulkan 1.3 Dynamic Rendering: vkCmdBeginRendering (No VkRenderPass!)
        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView   = s_ImageViews[s_CurrentImageIndex];
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color.float32[0] = static_cast<FLOAT>(s_ClearColor.r) / 255.0f;
        colorAttachment.clearValue.color.float32[1] = static_cast<FLOAT>(s_ClearColor.g) / 255.0f;
        colorAttachment.clearValue.color.float32[2] = static_cast<FLOAT>(s_ClearColor.b) / 255.0f;
        colorAttachment.clearValue.color.float32[3] = static_cast<FLOAT>(s_ClearColor.a) / 255.0f;

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea.offset    = { 0, 0 };
        renderingInfo.renderArea.extent    = s_Extent;
        renderingInfo.layerCount           = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments    = &colorAttachment;

        vkCmdBeginRendering(cmd, &renderingInfo);

        // Set dynamic Viewport & Scissor to match exact swapchain size
        VkViewport viewport{ 0.0f, 0.0f, static_cast<FLOAT>(s_Extent.width), static_cast<FLOAT>(s_Extent.height), 0.0f, 1.0f };
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        VkRect2D scissor{ { 0, 0 }, s_Extent };
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        return true;
    }

    // Finishes rendering, transitions image to PRESENT layout and displays to screen.
    // Example: R_Swapchain::EndFrame();
    static inline void EndFrame() {
        if (!s_IsInitialized) return;

        VkCommandBuffer cmd = s_CommandBuffers[s_CurrentFrameIndex];

        // 1. End Dynamic Rendering
        vkCmdEndRendering(cmd);

        // 2. Modern Vulkan 1.3 Transition: COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
        VkImageMemoryBarrier2 barrier{};
        barrier.sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        barrier.dstAccessMask = 0;
        barrier.oldLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.image         = s_Images[s_CurrentImageIndex];
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;

        VkDependencyInfo depInfo{};
        depInfo.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers    = &barrier;
        vkCmdPipelineBarrier2(cmd, &depInfo);

        vkEndCommandBuffer(cmd);

        // 3. Submit Command Buffer with Modern Synchronization2
        VkCommandBufferSubmitInfo cmdSubmitInfo{};
        cmdSubmitInfo.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        cmdSubmitInfo.commandBuffer = cmd;

        VkSemaphoreSubmitInfo waitSemInfo{};
        waitSemInfo.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        waitSemInfo.semaphore = s_ImageAvailableSemaphores[s_CurrentFrameIndex];
        waitSemInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSemaphoreSubmitInfo signalSemInfo{};
        signalSemInfo.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalSemInfo.semaphore = s_RenderFinishedSemaphores[s_CurrentImageIndex];
        signalSemInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo2 submitInfo{};
        submitInfo.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submitInfo.waitSemaphoreInfoCount   = 1;
        submitInfo.pWaitSemaphoreInfos      = &waitSemInfo;
        submitInfo.signalSemaphoreInfoCount = 1;
        submitInfo.pSignalSemaphoreInfos    = &signalSemInfo;
        submitInfo.commandBufferInfoCount   = 1;
        submitInfo.pCommandBufferInfos      = &cmdSubmitInfo;

        vkQueueSubmit2(R_Device::Get_GraphicsQueue(), 1, &submitInfo, s_InFlightFences[s_CurrentFrameIndex]);

        // 4. Present image to screen
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = &s_RenderFinishedSemaphores[s_CurrentImageIndex];
        presentInfo.swapchainCount     = 1;
        presentInfo.pSwapchains        = &s_Swapchain;
        presentInfo.pImageIndices      = &s_CurrentImageIndex;

        VkResult presentResult = vkQueuePresentKHR(R_Device::Get_PresentQueue(), &presentInfo);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || Window::Was_Resized()) {
            s_NeedsResize = true;
        }

        s_CurrentFrameIndex = (s_CurrentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    // ========================================================================
    // Setters & Getters
    // ========================================================================

    // Sets the background clear color (e.g. COLOR_BLUE, COLOR(30, 40, 55, 255)).
    // Example: R_Swapchain::Set_ClearColor(COLOR(20, 20, 30, 255));
    static inline void Set_ClearColor(const COLOR &color) {
        s_ClearColor = color;
    }

    // Returns the current command buffer for recording custom draw calls between BeginFrame & EndFrame!
    static inline VkCommandBuffer Get_CurrentCommandBuffer() {
        return s_CommandBuffers[s_CurrentFrameIndex];
    }

    // Turns VSync ON or OFF dynamically (recreates swapchain presentation mode on the fly).
    // Example: R_Swapchain::Set_VSync(false);
    static inline void Set_VSync(BOOL enabled) {
        if (s_VSync == enabled) return;
        s_VSync = enabled;
        Config::Set("Window", "VSync", enabled);
        s_NeedsResize = true; // Flags swapchain to recreate with new present mode!
        LOG(Info, LogLevel::Core, "RenderSwapchain: VSync set to {}", enabled ? "ON" : "OFF");
    }

    // Returns TRUE if VSync is currently enabled.
    // Example: BOOL vSync = R_Swapchain::Get_VSync();
    static inline BOOL Get_VSync() {
        return s_VSync;
    }

    static inline VkExtent2D Get_Extent()      { return s_Extent; }
    static inline VkFormat   Get_ImageFormat() { return s_ImageFormat; }
    static inline BOOL       Is_Initialized()  { return s_IsInitialized; }
};

using R_Swapchain = RenderSwapchain; // Short R_ prefix alias for Swapchain
