#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <U_Colors.h>
#include <M_Log.h>
#include <M_Window.h>

// ============================================================================
// Vulkan 1.3 / 1.4 Core Graphics Backend (Dynamic Rendering & Sync 2)
// ============================================================================
// Features:
//   - Pure Cross-Platform (Windows, Linux Wayland / X11 via SDL3)
//   - Modern Vulkan 1.3+ Dynamic Rendering (No VkRenderPass / VkFramebuffer bloat)
//   - Modern Synchronization 2 (vkCmdPipelineBarrier2 & vkQueueSubmit2)
//   - Triple Buffering CPU-GPU pipeline (MAX_FRAMES_IN_FLIGHT = 3)
//   - Dynamic VSync Toggle (IMMEDIATE for uncapped, FIFO for VSync)
// ============================================================================
class VulkanRenderer {
public:
    static constexpr UINT MAX_FRAMES_IN_FLIGHT = 3;

private:
    // Core Vulkan Device Handles
    static inline VkInstance               s_Instance               = VK_NULL_HANDLE;
    static inline VkDebugUtilsMessengerEXT s_DebugMessenger         = VK_NULL_HANDLE;
    static inline VkSurfaceKHR             s_Surface                = VK_NULL_HANDLE;
    static inline VkPhysicalDevice         s_PhysicalDevice         = VK_NULL_HANDLE;
    static inline VkDevice                 s_Device                 = VK_NULL_HANDLE;
    static inline VkQueue                  s_GraphicsQueue          = VK_NULL_HANDLE;
    static inline VkQueue                  s_PresentQueue           = VK_NULL_HANDLE;
    static inline UINT                     s_GraphicsQueueFamily    = 0;
    static inline UINT                     s_PresentQueueFamily     = 0;
    static inline VkCommandPool            s_CommandPool            = VK_NULL_HANDLE;
    static inline String                   s_GPUName                = "Unknown GPU";

    // Swapchain & Presentation Handles
    static inline VkSwapchainKHR           s_Swapchain              = VK_NULL_HANDLE;
    static inline VkFormat                 s_ImageFormat            = VK_FORMAT_B8G8R8A8_SRGB;
    static inline VkColorSpaceKHR          s_ColorSpace             = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    static inline VkExtent2D               s_Extent                 = { 1280, 720 };
    static inline Array<VkImage>           s_Images;
    static inline Array<VkImageView>       s_ImageViews;

    // Per-frame Synchronization Primitives
    static inline fArray<VkSemaphore, MAX_FRAMES_IN_FLIGHT>     s_ImageAvailableSemaphores{};
    static inline Array<VkSemaphore>                            s_RenderFinishedSemaphores;
    static inline fArray<VkFence, MAX_FRAMES_IN_FLIGHT>         s_InFlightFences{};
    static inline fArray<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> s_CommandBuffers{};

    // State Flags
    static inline UINT  s_CurrentFrameIndex = 0;
    static inline UINT  s_CurrentImageIndex = 0;
    static inline BOOL  s_IsInitialized     = false;
    static inline BOOL  s_NeedsResize       = false;
    static inline BOOL  s_VSync             = true;
    static inline COLOR s_ClearColor        = COLOR(30, 40, 55, 255);

#if defined(_RELEASE)
    static constexpr BOOL s_EnableValidationLayers = false;
#else
    static constexpr BOOL s_EnableValidationLayers = true;
#endif

    static inline const fArray<const Char *, 1> s_ValidationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    // ------------------------------------------------------------------------
    // Internal Helper Functions
    // ------------------------------------------------------------------------
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void *pUserData)
    {
        (void)type; (void)pUserData;
        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            LOG(Error, LogLevel::Core, "[Vulkan Validation] {}", pCallbackData->pMessage);
        } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            LOG(Warn, LogLevel::Core, "[Vulkan Validation] {}", pCallbackData->pMessage);
        }
        return VK_FALSE;
    }

    static inline BOOL CheckValidationLayerSupport() {
        UINT layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        Array<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const Char *layerName : s_ValidationLayers) {
            BOOL found = false;
            for (const auto &layer : availableLayers) {
                if (strcmp(layerName, layer.layerName) == 0) { found = true; break; }
            }
            if (!found) return false;
        }
        return true;
    }

    static inline VkSurfaceFormatKHR ChooseSurfaceFormat(const Array<VkSurfaceFormatKHR> &formats) {
        for (const auto &fmt : formats) {
            if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB && fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return fmt;
            }
        }
        return formats[0];
    }

    static inline VkPresentModeKHR ChoosePresentMode(const Array<VkPresentModeKHR> &modes, BOOL vSync) {
        if (!vSync) {
            for (const auto &m : modes) if (m == VK_PRESENT_MODE_IMMEDIATE_KHR) return m;
            for (const auto &m : modes) if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    static inline VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR &capabilities) {
        if (capabilities.currentExtent.width != UINT32_MAX) {
            return capabilities.currentExtent;
        }
        Vector2D px = Window::Get_PixelSize();
        VkExtent2D ext = { static_cast<UINT>(px.x), static_cast<UINT>(px.y) };
        ext.width  = std::clamp(ext.width,  capabilities.minImageExtent.width,  capabilities.maxImageExtent.width);
        ext.height = std::clamp(ext.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return ext;
    }

    static inline BOOL CreateSwapchain() {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s_PhysicalDevice, s_Surface, &capabilities);

        UINT formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(s_PhysicalDevice, s_Surface, &formatCount, nullptr);
        Array<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(s_PhysicalDevice, s_Surface, &formatCount, formats.data());

        UINT modeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(s_PhysicalDevice, s_Surface, &modeCount, nullptr);
        Array<VkPresentModeKHR> modes(modeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(s_PhysicalDevice, s_Surface, &modeCount, modes.data());

        VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(formats);
        VkPresentModeKHR   presentMode   = ChoosePresentMode(modes, s_VSync);

        s_Extent      = ChooseExtent(capabilities);
        s_ImageFormat = surfaceFormat.format;
        s_ColorSpace  = surfaceFormat.colorSpace;

        UINT imageCount = s_VSync ? (capabilities.minImageCount + 1) : (capabilities.minImageCount + 2);
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
            imageCount = capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface          = s_Surface;
        createInfo.minImageCount    = imageCount;
        createInfo.imageFormat      = s_ImageFormat;
        createInfo.imageColorSpace  = s_ColorSpace;
        createInfo.imageExtent      = s_Extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        UINT queueIndices[] = { s_GraphicsQueueFamily, s_PresentQueueFamily };
        if (queueIndices[0] != queueIndices[1]) {
            createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices   = queueIndices;
        } else {
            createInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
        }

        createInfo.preTransform   = capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode    = presentMode;
        createInfo.clipped        = VK_TRUE;
        createInfo.oldSwapchain   = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(s_Device, &createInfo, nullptr, &s_Swapchain) != VK_SUCCESS) {
            LOG(Error, LogLevel::Core, "Vulkan: Failed to create VkSwapchainKHR!");
            return false;
        }

        // Retrieve Images & Create Views
        UINT actualCount = 0;
        vkGetSwapchainImagesKHR(s_Device, s_Swapchain, &actualCount, nullptr);
        s_Images.resize(actualCount);
        vkGetSwapchainImagesKHR(s_Device, s_Swapchain, &actualCount, s_Images.data());

        s_ImageViews.resize(actualCount);
        for (size_t i = 0; i < actualCount; ++i) {
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

            if (vkCreateImageView(s_Device, &viewInfo, nullptr, &s_ImageViews[i]) != VK_SUCCESS) {
                LOG(Error, LogLevel::Core, "Vulkan: Failed to create image view [{}]!", i);
                return false;
            }
        }

        // Semaphores per Swapchain Image
        s_RenderFinishedSemaphores.resize(actualCount);
        VkSemaphoreCreateInfo semInfo{};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        for (size_t i = 0; i < actualCount; ++i) {
            if (vkCreateSemaphore(s_Device, &semInfo, nullptr, &s_RenderFinishedSemaphores[i]) != VK_SUCCESS) {
                return false;
            }
        }

        LOG(Info, LogLevel::Core, "Vulkan: Swapchain created ({}x{}, {} images, VSync: {})",
            s_Extent.width, s_Extent.height, actualCount, s_VSync ? "ON" : "OFF");
        return true;
    }

    static inline void CleanupSwapchain() {
        if (!s_Device) return;

        for (auto sem : s_RenderFinishedSemaphores) {
            if (sem) vkDestroySemaphore(s_Device, sem, nullptr);
        }
        s_RenderFinishedSemaphores.clear();

        for (auto view : s_ImageViews) {
            if (view) vkDestroyImageView(s_Device, view, nullptr);
        }
        s_ImageViews.clear();
        s_Images.clear();

        if (s_Swapchain) {
            vkDestroySwapchainKHR(s_Device, s_Swapchain, nullptr);
            s_Swapchain = VK_NULL_HANDLE;
        }
    }

public:
    // ========================================================================
    // Lifecycle (Init, Shutdown, Recreate)
    // ========================================================================

    // Initializes Vulkan 1.3/1.4 Device, Surface & Swapchain on the target window
    static inline BOOL Init(SDL_Window *window) {
        if (s_IsInitialized) return true;
        if (!window) {
            LOG(Error, LogLevel::Core, "Vulkan::Init failed: window handle is NULL!");
            return false;
        }

        s_VSync = Config::Get("Window", "VSync", true);

        // 1. Load Vulkan library dynamically via SDL3
        if (!SDL_Vulkan_LoadLibrary(nullptr)) {
            LOG(Error, LogLevel::Core, "Vulkan: Failed to load library: {}", SDL_GetError());
            return false;
        }

        // 2. Query extensions required by SDL3
        UINT sdlExtensionCount = 0;
        const char *const *sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
        Array<const Char *> extensions(sdlExtensions, sdlExtensions + sdlExtensionCount);
        if constexpr (s_EnableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        // 3. Create VkInstance (Targeting Vulkan 1.3 Core)
        VkApplicationInfo appInfo{};
        appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName   = "Aurora Engine";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName        = "AuroraEngine";
        appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion         = VK_API_VERSION_1_3;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo        = &appInfo;
        createInfo.enabledExtensionCount   = static_cast<UINT>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        if constexpr (s_EnableValidationLayers) {
            if (CheckValidationLayerSupport()) {
                createInfo.enabledLayerCount   = static_cast<UINT>(s_ValidationLayers.size());
                createInfo.ppEnabledLayerNames = s_ValidationLayers.data();
            }
        }

        if (vkCreateInstance(&createInfo, nullptr, &s_Instance) != VK_SUCCESS) {
            LOG(Error, LogLevel::Core, "Vulkan: Failed to create VkInstance!");
            return false;
        }

        // 4. Debug Messenger
        if constexpr (s_EnableValidationLayers) {
            auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(s_Instance, "vkCreateDebugUtilsMessengerEXT");
            if (func) {
                VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
                debugInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
                debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
                debugInfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
                debugInfo.pfnUserCallback = DebugCallback;
                func(s_Instance, &debugInfo, nullptr, &s_DebugMessenger);
            }
        }

        // 5. Create Surface via SDL3
        if (!SDL_Vulkan_CreateSurface(window, s_Instance, nullptr, &s_Surface)) {
            LOG(Error, LogLevel::Core, "Vulkan: Failed to create Surface: {}", SDL_GetError());
            Shutdown();
            return false;
        }

        // 6. Pick Physical GPU
        UINT deviceCount = 0;
        vkEnumeratePhysicalDevices(s_Instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            LOG(Error, LogLevel::Core, "Vulkan: No Vulkan-compatible GPUs found!");
            Shutdown();
            return false;
        }

        Array<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(s_Instance, &deviceCount, devices.data());
        s_PhysicalDevice = devices[0]; // Primary GPU

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(s_PhysicalDevice, &props);
        s_GPUName = props.deviceName;

        // 7. Queue Families
        UINT queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(s_PhysicalDevice, &queueFamilyCount, nullptr);
        Array<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(s_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

        for (UINT i = 0; i < queueFamilyCount; ++i) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) s_GraphicsQueueFamily = i;
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(s_PhysicalDevice, i, s_Surface, &presentSupport);
            if (presentSupport) s_PresentQueueFamily = i;
        }

        // 8. Create Logical Device with Dynamic Rendering & Synchronization 2
        Set<UINT> uniqueFamilies = { s_GraphicsQueueFamily, s_PresentQueueFamily };
        Array<VkDeviceQueueCreateInfo> queueInfos;
        FLOAT priority = 1.0f;

        for (UINT family : uniqueFamilies) {
            VkDeviceQueueCreateInfo qInfo{};
            qInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            qInfo.queueFamilyIndex = family;
            qInfo.queueCount       = 1;
            qInfo.pQueuePriorities = &priority;
            queueInfos.push_back(qInfo);
        }

        const Char *devExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features13.dynamicRendering = VK_TRUE;
        features13.synchronization2 = VK_TRUE;

        VkPhysicalDeviceFeatures devFeatures{};
        devFeatures.samplerAnisotropy = VK_TRUE;

        VkDeviceCreateInfo devCreateInfo{};
        devCreateInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        devCreateInfo.pNext                   = &features13;
        devCreateInfo.queueCreateInfoCount    = static_cast<UINT>(queueInfos.size());
        devCreateInfo.pQueueCreateInfos       = queueInfos.data();
        devCreateInfo.pEnabledFeatures        = &devFeatures;
        devCreateInfo.enabledExtensionCount   = 1;
        devCreateInfo.ppEnabledExtensionNames = devExtensions;

        if (vkCreateDevice(s_PhysicalDevice, &devCreateInfo, nullptr, &s_Device) != VK_SUCCESS) {
            LOG(Error, LogLevel::Core, "Vulkan: Failed to create VkDevice!");
            Shutdown();
            return false;
        }

        vkGetDeviceQueue(s_Device, s_GraphicsQueueFamily, 0, &s_GraphicsQueue);
        vkGetDeviceQueue(s_Device, s_PresentQueueFamily, 0, &s_PresentQueue);

        // 9. Command Pool & Command Buffers
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = s_GraphicsQueueFamily;

        if (vkCreateCommandPool(s_Device, &poolInfo, nullptr, &s_CommandPool) != VK_SUCCESS) {
            LOG(Error, LogLevel::Core, "Vulkan: Failed to create Command Pool!");
            Shutdown();
            return false;
        }

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = s_CommandPool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
        vkAllocateCommandBuffers(s_Device, &allocInfo, s_CommandBuffers.data());

        // 10. Frame Synchronization Primitives
        VkSemaphoreCreateInfo semInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VkFenceCreateInfo     fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT };

        for (UINT i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vkCreateSemaphore(s_Device, &semInfo, nullptr, &s_ImageAvailableSemaphores[i]);
            vkCreateFence(s_Device, &fenceInfo, nullptr, &s_InFlightFences[i]);
        }

        // 11. Create Swapchain
        if (!CreateSwapchain()) {
            Shutdown();
            return false;
        }

        s_IsInitialized = true;
        LOG(Info, LogLevel::Core, "Vulkan: Backend initialized successfully on [{}]", s_GPUName);
        return true;
    }

    // Shuts down all Vulkan resources cleanly
    static inline void Shutdown() {
        if (!s_IsInitialized && !s_Instance) return;

        if (s_Device) {
            vkDeviceWaitIdle(s_Device);

            for (UINT i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                if (s_ImageAvailableSemaphores[i]) {
                    vkDestroySemaphore(s_Device, s_ImageAvailableSemaphores[i], nullptr);
                    s_ImageAvailableSemaphores[i] = VK_NULL_HANDLE;
                }
                if (s_InFlightFences[i]) {
                    vkDestroyFence(s_Device, s_InFlightFences[i], nullptr);
                    s_InFlightFences[i] = VK_NULL_HANDLE;
                }
            }

            if (s_CommandPool) {
                vkFreeCommandBuffers(s_Device, s_CommandPool, MAX_FRAMES_IN_FLIGHT, s_CommandBuffers.data());
            }

            CleanupSwapchain();

            if (s_CommandPool) {
                vkDestroyCommandPool(s_Device, s_CommandPool, nullptr);
                s_CommandPool = VK_NULL_HANDLE;
            }

            vkDestroyDevice(s_Device, nullptr);
            s_Device = VK_NULL_HANDLE;
        }

        if (s_Surface) {
            vkDestroySurfaceKHR(s_Instance, s_Surface, nullptr);
            s_Surface = VK_NULL_HANDLE;
        }

        if (s_DebugMessenger) {
            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(s_Instance, "vkDestroyDebugUtilsMessengerEXT");
            if (func) func(s_Instance, s_DebugMessenger, nullptr);
            s_DebugMessenger = VK_NULL_HANDLE;
        }

        if (s_Instance) {
            vkDestroyInstance(s_Instance, nullptr);
            s_Instance = VK_NULL_HANDLE;
        }

        s_IsInitialized = false;
        LOG(Info, LogLevel::Core, "Vulkan: Backend shutdown cleanly.");
    }

    // Handles window resizing & minimizing smoothly
    static inline void Recreate() {
        Vector2D px = Window::Get_PixelSize();
        if (px.x <= 0 || px.y <= 0 || !s_Device) return;

        vkDeviceWaitIdle(s_Device);
        CleanupSwapchain();
        CreateSwapchain();
        s_NeedsResize = false;
    }

    // ========================================================================
    // Frame Rendering (Begin_Frame -> Draw -> End_Frame)
    // ========================================================================

    // Prepares the screen for drawing. Clears screen to s_ClearColor using Dynamic Rendering.
    static inline BOOL Begin_Frame() {
        if (!s_IsInitialized || Window::Is_Minimized()) return false;

        if (s_NeedsResize || Window::Was_Resized()) {
            Recreate();
            return false;
        }

        // 1. Wait for GPU to finish previous frame in this slot
        vkWaitForFences(s_Device, 1, &s_InFlightFences[s_CurrentFrameIndex], VK_TRUE, UINT64_MAX);

        // 2. Acquire next swapchain image
        VkResult acquireResult = vkAcquireNextImageKHR(
            s_Device,
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
            return false;
        }

        vkResetFences(s_Device, 1, &s_InFlightFences[s_CurrentFrameIndex]);

        // 3. Record Commands
        VkCommandBuffer cmd = s_CommandBuffers[s_CurrentFrameIndex];
        vkResetCommandBuffer(cmd, 0);

        VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        vkBeginCommandBuffer(cmd, &beginInfo);

        // 4. Transition: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL via Sync 2
        VkImageMemoryBarrier2 barrier{};
        barrier.sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.srcAccessMask = 0;
        barrier.dstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.image         = s_Images[s_CurrentImageIndex];
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        VkDependencyInfo depInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO, nullptr, 0, 0, nullptr, 0, nullptr, 1, &barrier };
        vkCmdPipelineBarrier2(cmd, &depInfo);

        // 5. Dynamic Rendering: vkCmdBeginRendering
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
        renderingInfo.renderArea           = { { 0, 0 }, s_Extent };
        renderingInfo.layerCount           = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments    = &colorAttachment;

        vkCmdBeginRendering(cmd, &renderingInfo);

        VkViewport viewport{ 0.0f, 0.0f, static_cast<FLOAT>(s_Extent.width), static_cast<FLOAT>(s_Extent.height), 0.0f, 1.0f };
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        VkRect2D scissor{ { 0, 0 }, s_Extent };
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        return true;
    }

    // Finishes rendering, transitions image to PRESENT layout and presents to screen
    static inline void End_Frame() {
        if (!s_IsInitialized) return;

        VkCommandBuffer cmd = s_CommandBuffers[s_CurrentFrameIndex];
        vkCmdEndRendering(cmd);

        // Transition: COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
        VkImageMemoryBarrier2 barrier{};
        barrier.sType         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        barrier.dstAccessMask = 0;
        barrier.oldLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.image         = s_Images[s_CurrentImageIndex];
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        VkDependencyInfo depInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO, nullptr, 0, 0, nullptr, 0, nullptr, 1, &barrier };
        vkCmdPipelineBarrier2(cmd, &depInfo);
        vkEndCommandBuffer(cmd);

        // Submit via Sync 2
        VkCommandBufferSubmitInfo cmdSubmitInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, nullptr, cmd };
        VkSemaphoreSubmitInfo waitSemInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, nullptr, s_ImageAvailableSemaphores[s_CurrentFrameIndex], 0, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkSemaphoreSubmitInfo signalSemInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, nullptr, s_RenderFinishedSemaphores[s_CurrentImageIndex], 0, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT };

        VkSubmitInfo2 submitInfo{};
        submitInfo.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submitInfo.waitSemaphoreInfoCount   = 1;
        submitInfo.pWaitSemaphoreInfos      = &waitSemInfo;
        submitInfo.signalSemaphoreInfoCount = 1;
        submitInfo.pSignalSemaphoreInfos    = &signalSemInfo;
        submitInfo.commandBufferInfoCount   = 1;
        submitInfo.pCommandBufferInfos      = &cmdSubmitInfo;

        vkQueueSubmit2(s_GraphicsQueue, 1, &submitInfo, s_InFlightFences[s_CurrentFrameIndex]);

        // Present
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = &s_RenderFinishedSemaphores[s_CurrentImageIndex];
        presentInfo.swapchainCount     = 1;
        presentInfo.pSwapchains        = &s_Swapchain;
        presentInfo.pImageIndices      = &s_CurrentImageIndex;

        VkResult presentResult = vkQueuePresentKHR(s_PresentQueue, &presentInfo);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || Window::Was_Resized()) {
            s_NeedsResize = true;
        }

        s_CurrentFrameIndex = (s_CurrentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    // ========================================================================
    // Setters & Getters
    // ========================================================================
    static inline void Set_VSync(BOOL enabled) {
        if (s_VSync == enabled) return;
        s_VSync = enabled;
        Config::Set("Window", "VSync", enabled);
        s_NeedsResize = true;
    }

    static inline BOOL       Get_VSync()                     { return s_VSync; }
    static inline void       Set_ClearColor(const COLOR &c)  { s_ClearColor = c; }
    static inline COLOR      Get_ClearColor()                { return s_ClearColor; }
    static inline StringView Get_GPUName()                   { return s_GPUName; }
    static inline BOOL       Is_Initialized()                { return s_IsInitialized; }
    static inline VkCommandBuffer Get_CurrentCommandBuffer() { return s_CommandBuffers[s_CurrentFrameIndex]; }
};

using R_Vulkan = VulkanRenderer; // Short alias
