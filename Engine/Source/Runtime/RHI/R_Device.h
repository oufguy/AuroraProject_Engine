#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <M_Log.h>

// ============================================================================
// Modern Vulkan 1.3+ Render Device (Instance, Surface, Physical & Logical GPU)
// ============================================================================
// Features Enabled:
//   - Vulkan 1.3 Core: Dynamic Rendering (No VkRenderPass / VkFramebuffer bloat!)
//   - Vulkan 1.3 Core: Synchronization 2 (Clean image transitions)
//   - Anisotropic Filtering (Crisp textures at sharp angles)
//   - Cross-Platform Surface (Auto-configured by SDL3 for Windows, X11 & Wayland)
// ============================================================================
class RenderDevice {
private:
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

    static inline String s_GPUName      = "Unknown GPU";
    static inline BOOL   s_IsInitialized = false;

    // Device extensions required for swapchain presentation
    static inline const fArray<const Char *, 1> s_RequiredDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    // Validation layers enabled only in Debug and Development builds
#if defined(_RELEASE)
    static constexpr BOOL s_EnableValidationLayers = false;
#else
    static constexpr BOOL s_EnableValidationLayers = true;
#endif

    static inline const fArray<const Char *, 1> s_ValidationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    // Debug callback for Vulkan validation layers
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void *pUserData)
    {
        (void)type;
        (void)pUserData;
        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            LOG(Error, LogLevel::Core, "[Vulkan Validation] {}", pCallbackData->pMessage);
        } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            LOG(Warn, LogLevel::Core, "[Vulkan Validation] {}", pCallbackData->pMessage);
        }
        return VK_FALSE;
    }

    // Checks if the requested validation layers are available on this PC
    static inline BOOL CheckValidationLayerSupport() {
        UINT layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        Array<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const Char *layerName : s_ValidationLayers) {
            BOOL layerFound = false;
            for (const auto &layerProps : availableLayers) {
                if (strcmp(layerName, layerProps.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }
            if (!layerFound) return false;
        }
        return true;
    }

    // Rate a physical GPU (discrete GPU like NVIDIA/AMD gets high score)
    static inline INT RateDeviceSuitability(VkPhysicalDevice device) {
        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures   deviceFeatures;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        INT score = 0;

        // Dedicated graphics cards have huge performance advantage
        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 10000;
        } else if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            score += 1000;
        }

        // Must support geometry shaders and anisotropic filtering
        if (!deviceFeatures.samplerAnisotropy) return 0;

        // Check if Vulkan 1.3 is supported
        if (deviceProperties.apiVersion < VK_API_VERSION_1_3) return 0;

        return score;
    }

public:
    // ========================================================================
    // Init & Shutdown
    // ========================================================================

    // Initializes Vulkan 1.3, creates the OS window surface, picks GPU & creates VkDevice.
    // Example: R_Device::Init(Window::Get_Handle(), "Aurora Client");
    static inline BOOL Init(SDL_Window *window, StringView appName = "Aurora Client") {
        if (s_IsInitialized) return true;
        if (!window) {
            LOG(Error, LogLevel::Core, "RenderDevice::Init failed: window handle is NULL!");
            return false;
        }

        // 1. Check Vulkan library loader from SDL3
        if (!SDL_Vulkan_LoadLibrary(nullptr)) {
            LOG(Error, LogLevel::Core, "RenderDevice::Init failed to load Vulkan library: {}", SDL_GetError());
            return false;
        }

        // 2. Query instance extensions required by SDL3 for this OS (Windows / Wayland / X11)
        UINT sdlExtensionCount = 0;
        const char *const *sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
        if (!sdlExtensions) {
            LOG(Error, LogLevel::Core, "RenderDevice::Init failed to get SDL Vulkan extensions: {}", SDL_GetError());
            return false;
        }

        Array<const Char *> extensions(sdlExtensions, sdlExtensions + sdlExtensionCount);
        if constexpr (s_EnableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        // 3. Create VkInstance requesting Vulkan 1.3
        VkApplicationInfo appInfo{};
        appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName   = String(appName).c_str();
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
            } else {
                LOG(Warn, LogLevel::Core, "RenderDevice: Vulkan validation layers requested, but not available!");
            }
        }

        if (vkCreateInstance(&createInfo, nullptr, &s_Instance) != VK_SUCCESS) {
            LOG(Error, LogLevel::Core, "RenderDevice::Init failed to create VkInstance!");
            return false;
        }

        // 4. Setup Debug Messenger if validation layers are enabled
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

        // 5. Create cross-platform Window Surface via SDL3
        if (!SDL_Vulkan_CreateSurface(window, s_Instance, nullptr, &s_Surface)) {
            LOG(Error, LogLevel::Core, "RenderDevice::Init failed to create Vulkan surface: {}", SDL_GetError());
            Shutdown();
            return false;
        }

        // 6. Select Physical Device (GPU)
        UINT deviceCount = 0;
        vkEnumeratePhysicalDevices(s_Instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            LOG(Error, LogLevel::Core, "RenderDevice::Init found 0 Vulkan-compatible GPUs!");
            Shutdown();
            return false;
        }

        Array<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(s_Instance, &deviceCount, devices.data());

        INT bestScore = -1;
        for (const auto &dev : devices) {
            INT score = RateDeviceSuitability(dev);
            if (score > bestScore) {
                bestScore        = score;
                s_PhysicalDevice = dev;
            }
        }

        if (s_PhysicalDevice == VK_NULL_HANDLE) {
            LOG(Error, LogLevel::Core, "RenderDevice::Init failed to find a suitable GPU!");
            Shutdown();
            return false;
        }

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(s_PhysicalDevice, &props);
        s_GPUName = props.deviceName;

        // 7. Find Queue Families (Graphics & Present)
        UINT queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(s_PhysicalDevice, &queueFamilyCount, nullptr);
        Array<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(s_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

        BOOL foundGraphics = false;
        BOOL foundPresent  = false;

        for (UINT i = 0; i < queueFamilyCount; ++i) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                s_GraphicsQueueFamily = i;
                foundGraphics         = true;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(s_PhysicalDevice, i, s_Surface, &presentSupport);
            if (presentSupport) {
                s_PresentQueueFamily = i;
                foundPresent         = true;
            }

            if (foundGraphics && foundPresent) break;
        }

        if (!foundGraphics || !foundPresent) {
            LOG(Error, LogLevel::Core, "RenderDevice::Init failed to find Graphics or Present queue family!");
            Shutdown();
            return false;
        }

        // 8. Create Logical Device with Vulkan 1.3 Dynamic Rendering & Synchronization2
        Set<UINT> uniqueQueueFamilies = { s_GraphicsQueueFamily, s_PresentQueueFamily };
        Array<VkDeviceQueueCreateInfo> queueCreateInfos;
        FLOAT queuePriority = 1.0f;

        for (UINT queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount       = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        Array<const Char *> enabledDeviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        // Enable modern Vulkan 1.3 features: Dynamic Rendering & Synchronization2
        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features13.dynamicRendering = VK_TRUE; // NO VkRenderPass needed!
        features13.synchronization2 = VK_TRUE; // Modern clean barriers

        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE; // 16x anisotropic filtering

        VkDeviceCreateInfo devCreateInfo{};
        devCreateInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        devCreateInfo.pNext                   = &features13;
        devCreateInfo.queueCreateInfoCount    = static_cast<UINT>(queueCreateInfos.size());
        devCreateInfo.pQueueCreateInfos       = queueCreateInfos.data();
        devCreateInfo.pEnabledFeatures        = &deviceFeatures;
        devCreateInfo.enabledExtensionCount   = static_cast<UINT>(enabledDeviceExtensions.size());
        devCreateInfo.ppEnabledExtensionNames = enabledDeviceExtensions.data();

        if (vkCreateDevice(s_PhysicalDevice, &devCreateInfo, nullptr, &s_Device) != VK_SUCCESS) {
            LOG(Error, LogLevel::Core, "RenderDevice::Init failed to create VkDevice!");
            Shutdown();
            return false;
        }

        // 9. Retrieve Queues
        vkGetDeviceQueue(s_Device, s_GraphicsQueueFamily, 0, &s_GraphicsQueue);
        vkGetDeviceQueue(s_Device, s_PresentQueueFamily, 0, &s_PresentQueue);

        // 10. Create Graphics Command Pool
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = s_GraphicsQueueFamily;

        if (vkCreateCommandPool(s_Device, &poolInfo, nullptr, &s_CommandPool) != VK_SUCCESS) {
            LOG(Error, LogLevel::Core, "RenderDevice::Init failed to create VkCommandPool!");
            Shutdown();
            return false;
        }

        s_IsInitialized = true;
        LOG(Info, LogLevel::Core, "RenderDevice::Init successfully created Vulkan 1.3 device on [{}]", s_GPUName);
        return true;
    }

    // Shuts down Vulkan device and destroys all resources cleanly.
    // Example: R_Device::Shutdown();
    static inline void Shutdown() {
        if (!s_Instance) return;

        if (s_Device) {
            vkDeviceWaitIdle(s_Device);

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

        vkDestroyInstance(s_Instance, nullptr);
        s_Instance = VK_NULL_HANDLE;

        s_IsInitialized = false;
        LOG(Info, LogLevel::Core, "RenderDevice::Shutdown cleaned up Vulkan resources.");
    }

    // ========================================================================
    // Getters
    // ========================================================================
    static inline VkDevice         Get_Device()              { return s_Device; }
    static inline VkPhysicalDevice Get_PhysicalDevice()      { return s_PhysicalDevice; }
    static inline VkSurfaceKHR     Get_Surface()             { return s_Surface; }
    static inline VkQueue          Get_GraphicsQueue()       { return s_GraphicsQueue; }
    static inline VkQueue          Get_PresentQueue()        { return s_PresentQueue; }
    static inline UINT             Get_GraphicsQueueFamily() { return s_GraphicsQueueFamily; }
    static inline UINT             Get_PresentQueueFamily()  { return s_PresentQueueFamily; }
    static inline VkCommandPool    Get_CommandPool()         { return s_CommandPool; }
    static inline StringView       Get_GPUName()             { return s_GPUName; }
    static inline BOOL             Is_Initialized()          { return s_IsInitialized; }
};

using R_Device = RenderDevice; // Short R_ prefix alias for RHI Device
