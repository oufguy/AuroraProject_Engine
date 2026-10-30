#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <U_Colors.h>
#include <M_Log.h>
#include <M_Window.h>
#include <M_Mesh.h>
#include <M_Texture.h>
#include <S_Mesh.h>

// ============================================================================
// Vulkan 1.3 / 1.4 Core Graphics Backend (Dynamic Rendering & Sync 2)
// ============================================================================
// Features:
//   - Pure Cross-Platform (Windows, Linux Wayland / X11 via SDL3)
//   - Modern Vulkan 1.3+ Dynamic Rendering (No VkRenderPass / VkFramebuffer bloat)
//   - Modern Synchronization 2 (vkCmdPipelineBarrier2 & vkQueueSubmit2)
//   - Triple Buffering CPU-GPU pipeline (MAX_FRAMES_IN_FLIGHT = 3)
//   - Dynamic VSync Toggle (IMMEDIATE for uncapped, FIFO for VSync)
//   - Built-in 3D/2D Mesh Pipeline with Alpha Blending, Textures & Push Constants
// ============================================================================
class R_Vulkan {
public:
    static constexpr UINT MAX_FRAMES_IN_FLIGHT   = 3;
    static constexpr UINT MAX_VERTICES_PER_FRAME = 65536;
    static constexpr UINT MAX_INDICES_PER_FRAME  = 131072;
    static constexpr UINT STATIC_VERTEX_CAPACITY = 32768; // First half reserved for zero-copy cached static meshes
    static constexpr UINT STATIC_INDEX_CAPACITY  = 65536;

private:
    struct CachedMeshSlot {
        UINT  VertexOffset = 0;
        UINT  IndexOffset  = 0;
        UINT  VertexCount  = 0;
        UINT  IndexCount   = 0;
        UINT  Revision     = 0;
        DWORD FirstColor   = 0;
    };

    struct GPUTextureSlot {
        VkImage         Image         = VK_NULL_HANDLE;
        VkDeviceMemory  Memory        = VK_NULL_HANDLE;
        VkImageView     View          = VK_NULL_HANDLE;
        VkDescriptorSet DescriptorSet = VK_NULL_HANDLE;
        UINT            Revision      = 0;
    };
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

    // Swapchain, Depth Buffer & Presentation Handles
    static inline VkSwapchainKHR           s_Swapchain              = VK_NULL_HANDLE;
    static inline VkFormat                 s_ImageFormat            = VK_FORMAT_B8G8R8A8_UNORM;
    static inline VkFormat                 s_DepthFormat            = VK_FORMAT_D32_SFLOAT;
    static inline VkColorSpaceKHR          s_ColorSpace             = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    static inline VkExtent2D               s_Extent                 = { 1280, 720 };
    static inline FLOAT                    s_AspectRatio            = 16.0f / 9.0f;
    static inline Array<VkImage>                              s_Images;
    static inline Array<VkImageView>                          s_ImageViews;
    static inline fArray<VkImage, MAX_FRAMES_IN_FLIGHT>        s_DepthImages{};
    static inline fArray<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> s_DepthMemories{};
    static inline fArray<VkImageView, MAX_FRAMES_IN_FLIGHT>    s_DepthImageViews{};

    // Graphics Pipeline, Texture Descriptors & Per-Frame Dynamic Mesh Buffers
    static inline VkDescriptorSetLayout    s_DescriptorSetLayout    = VK_NULL_HANDLE;
    static inline VkDescriptorPool         s_DescriptorPool         = VK_NULL_HANDLE;
    static inline VkSampler                s_TextureSampler         = VK_NULL_HANDLE;
    static inline uMap<UINT, GPUTextureSlot> s_TextureCache;
    static inline UINT                     s_LastBoundTextureID     = 0xFFFFFFFFu;
    static inline VkPipelineLayout         s_PipelineLayout         = VK_NULL_HANDLE;
    static inline VkPipeline               s_Pipeline               = VK_NULL_HANDLE;
    static inline fArray<VkBuffer, MAX_FRAMES_IN_FLIGHT>       s_VertexBuffers{};
    static inline fArray<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> s_VertexMemories{};
    static inline fArray<Vertex *, MAX_FRAMES_IN_FLIGHT>       s_MappedVertices{};
    static inline fArray<VkBuffer, MAX_FRAMES_IN_FLIGHT>       s_IndexBuffers{};
    static inline fArray<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> s_IndexMemories{};
    static inline fArray<UINT *, MAX_FRAMES_IN_FLIGHT>         s_MappedIndices{};
    static inline fArray<uMap<UINT, CachedMeshSlot>, MAX_FRAMES_IN_FLIGHT> s_MeshCache{};
    static inline fArray<UINT, MAX_FRAMES_IN_FLIGHT>           s_StaticVertexCursor{};
    static inline fArray<UINT, MAX_FRAMES_IN_FLIGHT>           s_StaticIndexCursor{};
    static inline UINT                                         s_FrameVertexOffset = STATIC_VERTEX_CAPACITY;
    static inline UINT                                         s_FrameIndexOffset  = STATIC_INDEX_CAPACITY;

    // Per-frame Synchronization Primitives
    static inline fArray<VkSemaphore, MAX_FRAMES_IN_FLIGHT>     s_ImageAvailableSemaphores{};
    static inline Array<VkSemaphore>                            s_RenderFinishedSemaphores;
    static inline fArray<VkFence, MAX_FRAMES_IN_FLIGHT>         s_InFlightFences{};
    static inline fArray<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> s_CommandBuffers{};

    // State Flags
    static inline UINT  s_CurrentFrameIndex = 0;
    static inline UINT  s_CurrentImageIndex = 0;
    static inline BOOL  s_IsInitialized     = false;
    static inline BOOL  s_IsFrameActive     = false;
    static inline BOOL  s_NeedsResize       = false;
    static inline BOOL  s_VSync             = true;
    static inline COLOR s_ClearColor        = COLOR(30, 40, 55, 255);

    // Validation layers add 40-50% CPU/GPU overhead; disabled by default for maximum FPS
    // (To debug Vulkan API errors, change to true)
    static constexpr BOOL s_EnableValidationLayers = false;

    static inline const fArray<const Char *, 1> s_ValidationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    // Mesh Shader Bytecode is cleanly loaded from Engine::Shaders::Mesh (see Shaders/S_Mesh.h)

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
        // Prefer UNORM so COLOR(r, g, b, a) matches DirectX 11 pixel-for-pixel
        for (const auto &fmt : formats) {
            if ((fmt.format == VK_FORMAT_B8G8R8A8_UNORM || fmt.format == VK_FORMAT_R8G8B8A8_UNORM) &&
                fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
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
        if (capabilities.currentExtent.width != UINT32_MAX && capabilities.currentExtent.width > 0 && capabilities.currentExtent.height > 0) {
            return capabilities.currentExtent;
        }
        Vector2D px = M_Window::Get_PixelSize();
        VkExtent2D ext = { static_cast<UINT>(px.x), static_cast<UINT>(px.y) };
        ext.width  = std::clamp(ext.width,  std::max(1u, capabilities.minImageExtent.width),  std::max(1u, capabilities.maxImageExtent.width));
        ext.height = std::clamp(ext.height, std::max(1u, capabilities.minImageExtent.height), std::max(1u, capabilities.maxImageExtent.height));
        return ext;
    }

    static inline UINT FindMemoryType(UINT typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(s_PhysicalDevice, &memProps);
        for (UINT i = 0; i < memProps.memoryTypeCount; ++i) {
            if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        return 0;
    }

    static inline BOOL CreateHostBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer &outBuffer, VkDeviceMemory &outMemory, void **outMapped) {
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size        = size;
        bufInfo.usage       = usage;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(s_Device, &bufInfo, nullptr, &outBuffer) != VK_SUCCESS) return false;

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(s_Device, outBuffer, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize  = memReqs.size;
        allocInfo.memoryTypeIndex = FindMemoryType(
            memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(s_Device, &allocInfo, nullptr, &outMemory) != VK_SUCCESS) return false;
        if (vkBindBufferMemory(s_Device, outBuffer, outMemory, 0) != VK_SUCCESS) return false;
        if (vkMapMemory(s_Device, outMemory, 0, size, 0, outMapped) != VK_SUCCESS) return false;

        return true;
    }

    static inline BOOL CreateMeshPipelineAndBuffers() {
        // 1. Shader Modules
        VkShaderModuleCreateInfo vertInfo{};
        vertInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        vertInfo.codeSize = sizeof(Engine::Shaders::Mesh::Vulkan_Vertex_SPIRV);
        vertInfo.pCode    = Engine::Shaders::Mesh::Vulkan_Vertex_SPIRV;

        VkShaderModule vertModule = VK_NULL_HANDLE;
        if (vkCreateShaderModule(s_Device, &vertInfo, nullptr, &vertModule) != VK_SUCCESS) return false;

        VkShaderModuleCreateInfo fragInfo{};
        fragInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        fragInfo.codeSize = sizeof(Engine::Shaders::Mesh::Vulkan_Fragment_SPIRV);
        fragInfo.pCode    = Engine::Shaders::Mesh::Vulkan_Fragment_SPIRV;

        VkShaderModule fragModule = VK_NULL_HANDLE;
        if (vkCreateShaderModule(s_Device, &fragInfo, nullptr, &fragModule) != VK_SUCCESS) {
            vkDestroyShaderModule(s_Device, vertModule, nullptr);
            return false;
        }

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertModule;
        stages[0].pName  = "main";

        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragModule;
        stages[1].pName  = "main";

        // 2. Vertex Input Layout (Vertex: Position, Normal, UV, Color)
        VkVertexInputBindingDescription binding{};
        binding.binding   = 0;
        binding.stride    = sizeof(Vertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attrs[4]{};
        attrs[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Position) };
        attrs[1] = { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Normal)   };
        attrs[2] = { 2, 0, VK_FORMAT_R32G32_SFLOAT,    offsetof(Vertex, UV)       };
        attrs[3] = { 3, 0, VK_FORMAT_R8G8B8A8_UNORM,   offsetof(Vertex, Color)    };

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount   = 1;
        vertexInput.pVertexBindingDescriptions      = &binding;
        vertexInput.vertexAttributeDescriptionCount = 4;
        vertexInput.pVertexAttributeDescriptions    = attrs;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount  = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode    = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace   = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.lineWidth   = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // 3. Alpha Blending (Supports smooth Entity Transparency 0.0 -> 1.0!)
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable         = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments    = &colorBlendAttachment;

        VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = 2;
        dynamicState.pDynamicStates    = dynamicStates;

        // 4. Texture Descriptor Layout, Pool & Sampler
        VkDescriptorSetLayoutBinding samplerBinding{};
        samplerBinding.binding            = 0;
        samplerBinding.descriptorCount    = 1;
        samplerBinding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerBinding.pImmutableSamplers = nullptr;
        samplerBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo dslInfo{};
        dslInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dslInfo.bindingCount = 1;
        dslInfo.pBindings    = &samplerBinding;

        if (vkCreateDescriptorSetLayout(s_Device, &dslInfo, nullptr, &s_DescriptorSetLayout) != VK_SUCCESS) {
            vkDestroyShaderModule(s_Device, fragModule, nullptr);
            vkDestroyShaderModule(s_Device, vertModule, nullptr);
            return false;
        }

        VkDescriptorPoolSize poolSize{};
        poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 512;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes    = &poolSize;
        poolInfo.maxSets       = 512;

        if (vkCreateDescriptorPool(s_Device, &poolInfo, nullptr, &s_DescriptorPool) != VK_SUCCESS) {
            vkDestroyShaderModule(s_Device, fragModule, nullptr);
            vkDestroyShaderModule(s_Device, vertModule, nullptr);
            return false;
        }

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter               = VK_FILTER_LINEAR;
        samplerInfo.minFilter               = VK_FILTER_LINEAR;
        samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable        = VK_FALSE;
        samplerInfo.maxAnisotropy           = 1.0f;
        samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable           = VK_FALSE;
        samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        if (vkCreateSampler(s_Device, &samplerInfo, nullptr, &s_TextureSampler) != VK_SUCCESS) {
            vkDestroyShaderModule(s_Device, fragModule, nullptr);
            vkDestroyShaderModule(s_Device, vertModule, nullptr);
            return false;
        }

        // 5. Push Constants & Pipeline Layout (DrawConstants = 96 bytes + Set 0 Texture Sampler)
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushRange.offset     = 0;
        pushRange.size       = sizeof(DrawConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount         = 1;
        layoutInfo.pSetLayouts            = &s_DescriptorSetLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges    = &pushRange;

        if (vkCreatePipelineLayout(s_Device, &layoutInfo, nullptr, &s_PipelineLayout) != VK_SUCCESS) {
            vkDestroyShaderModule(s_Device, fragModule, nullptr);
            vkDestroyShaderModule(s_Device, vertModule, nullptr);
            return false;
        }

        // 5. Depth Stencil State & Dynamic Rendering Pipeline Info (Vulkan 1.3 Core)
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable       = VK_TRUE;
        depthStencil.depthWriteEnable      = VK_TRUE;
        depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable     = VK_FALSE;

        VkPipelineRenderingCreateInfo renderingPipelineInfo{};
        renderingPipelineInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingPipelineInfo.colorAttachmentCount    = 1;
        renderingPipelineInfo.pColorAttachmentFormats = &s_ImageFormat;
        renderingPipelineInfo.depthAttachmentFormat   = s_DepthFormat;

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.pNext               = &renderingPipelineInfo;
        pipelineInfo.stageCount          = 2;
        pipelineInfo.pStages             = stages;
        pipelineInfo.pVertexInputState   = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState      = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState   = &multisampling;
        pipelineInfo.pDepthStencilState  = &depthStencil;
        pipelineInfo.pColorBlendState    = &colorBlending;
        pipelineInfo.pDynamicState       = &dynamicState;
        pipelineInfo.layout              = s_PipelineLayout;

        VkResult res = vkCreateGraphicsPipelines(s_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &s_Pipeline);
        vkDestroyShaderModule(s_Device, fragModule, nullptr);
        vkDestroyShaderModule(s_Device, vertModule, nullptr);
        if (res != VK_SUCCESS) return false;

        // 6. Per-Frame Mapped Vertex & Index Buffers
        for (UINT i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            void *mappedV = nullptr;
            if (!CreateHostBuffer(MAX_VERTICES_PER_FRAME * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, s_VertexBuffers[i], s_VertexMemories[i], &mappedV)) {
                return false;
            }
            s_MappedVertices[i] = static_cast<Vertex *>(mappedV);

            void *mappedI = nullptr;
            if (!CreateHostBuffer(MAX_INDICES_PER_FRAME * sizeof(UINT), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, s_IndexBuffers[i], s_IndexMemories[i], &mappedI)) {
                return false;
            }
            s_MappedIndices[i] = static_cast<UINT *>(mappedI);
        }

        return true;
    }

    static inline void DestroyMeshPipelineAndBuffers() {
        if (!s_Device) return;

        for (UINT i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            s_MeshCache[i].clear();
            s_StaticVertexCursor[i] = 0;
            s_StaticIndexCursor[i]  = 0;

            if (s_VertexMemories[i]) {
                vkUnmapMemory(s_Device, s_VertexMemories[i]);
                s_MappedVertices[i] = nullptr;
            }
            if (s_VertexBuffers[i]) {
                vkDestroyBuffer(s_Device, s_VertexBuffers[i], nullptr);
                s_VertexBuffers[i] = VK_NULL_HANDLE;
            }
            if (s_VertexMemories[i]) {
                vkFreeMemory(s_Device, s_VertexMemories[i], nullptr);
                s_VertexMemories[i] = VK_NULL_HANDLE;
            }

            if (s_IndexMemories[i]) {
                vkUnmapMemory(s_Device, s_IndexMemories[i]);
                s_MappedIndices[i] = nullptr;
            }
            if (s_IndexBuffers[i]) {
                vkDestroyBuffer(s_Device, s_IndexBuffers[i], nullptr);
                s_IndexBuffers[i] = VK_NULL_HANDLE;
            }
            if (s_IndexMemories[i]) {
                vkFreeMemory(s_Device, s_IndexMemories[i], nullptr);
                s_IndexMemories[i] = VK_NULL_HANDLE;
            }
        }

        if (s_Pipeline) {
            vkDestroyPipeline(s_Device, s_Pipeline, nullptr);
            s_Pipeline = VK_NULL_HANDLE;
        }
        if (s_PipelineLayout) {
            vkDestroyPipelineLayout(s_Device, s_PipelineLayout, nullptr);
            s_PipelineLayout = VK_NULL_HANDLE;
        }

        for (auto &kv : s_TextureCache) {
            if (kv.second.View)   vkDestroyImageView(s_Device, kv.second.View, nullptr);
            if (kv.second.Image)  vkDestroyImage(s_Device, kv.second.Image, nullptr);
            if (kv.second.Memory) vkFreeMemory(s_Device, kv.second.Memory, nullptr);
        }
        s_TextureCache.clear();
        s_LastBoundTextureID = 0xFFFFFFFFu;

        if (s_TextureSampler) {
            vkDestroySampler(s_Device, s_TextureSampler, nullptr);
            s_TextureSampler = VK_NULL_HANDLE;
        }
        if (s_DescriptorPool) {
            vkDestroyDescriptorPool(s_Device, s_DescriptorPool, nullptr);
            s_DescriptorPool = VK_NULL_HANDLE;
        }
        if (s_DescriptorSetLayout) {
            vkDestroyDescriptorSetLayout(s_Device, s_DescriptorSetLayout, nullptr);
            s_DescriptorSetLayout = VK_NULL_HANDLE;
        }
    }

    static inline VkDescriptorSet GetOrUploadTextureSet(UINT textureID) {
        if (!s_Device || !s_DescriptorPool || !s_DescriptorSetLayout || !s_TextureSampler) return VK_NULL_HANDLE;

        // Fast-path: Check GPU texture cache first with zero mutex locks
        auto it = s_TextureCache.find(textureID);
        if (it != s_TextureCache.end() && it->second.DescriptorSet != VK_NULL_HANDLE) {
            return it->second.DescriptorSet;
        }

        const TextureData &tex = M_Texture::Get(textureID);
        if (!tex.Is_Valid()) return VK_NULL_HANDLE;

        if (it != s_TextureCache.end()) {
            vkDeviceWaitIdle(s_Device);
            if (it->second.DescriptorSet) vkFreeDescriptorSets(s_Device, s_DescriptorPool, 1, &it->second.DescriptorSet);
            if (it->second.View)          vkDestroyImageView(s_Device, it->second.View, nullptr);
            if (it->second.Image)         vkDestroyImage(s_Device, it->second.Image, nullptr);
            if (it->second.Memory)        vkFreeMemory(s_Device, it->second.Memory, nullptr);
            s_TextureCache.erase(it);
        }

        VkDeviceSize imageSize = static_cast<VkDeviceSize>(tex.Width) * tex.Height * sizeof(COLOR);

        // 1. Create temporary host staging buffer
        VkBuffer       stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
        void          *mappedStaging = nullptr;
        if (!CreateHostBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, stagingMemory, &mappedStaging)) {
            return VK_NULL_HANDLE;
        }
        std::memcpy(mappedStaging, tex.Pixels.data(), static_cast<size_t>(imageSize));
        vkUnmapMemory(s_Device, stagingMemory);

        // 2. Create optimal GPU 2D Image
        VkImageCreateInfo imageInfo{};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width  = tex.Width;
        imageInfo.extent.height = tex.Height;
        imageInfo.extent.depth  = 1;
        imageInfo.mipLevels     = 1;
        imageInfo.arrayLayers   = 1;
        imageInfo.format        = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;

        GPUTextureSlot slot{};
        slot.Revision = tex.Revision;

        if (vkCreateImage(s_Device, &imageInfo, nullptr, &slot.Image) != VK_SUCCESS) {
            vkDestroyBuffer(s_Device, stagingBuffer, nullptr);
            vkFreeMemory(s_Device, stagingMemory, nullptr);
            return VK_NULL_HANDLE;
        }

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(s_Device, slot.Image, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize  = memReqs.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(s_Device, &allocInfo, nullptr, &slot.Memory) != VK_SUCCESS) {
            vkDestroyImage(s_Device, slot.Image, nullptr);
            vkDestroyBuffer(s_Device, stagingBuffer, nullptr);
            vkFreeMemory(s_Device, stagingMemory, nullptr);
            return VK_NULL_HANDLE;
        }
        vkBindImageMemory(s_Device, slot.Image, slot.Memory, 0);

        // 3. Copy Staging Buffer -> VkImage via one-shot command buffer
        VkCommandBufferAllocateInfo cmdAlloc{};
        cmdAlloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAlloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAlloc.commandPool        = s_CommandPool;
        cmdAlloc.commandBufferCount = 1;

        VkCommandBuffer uploadCmd = VK_NULL_HANDLE;
        vkAllocateCommandBuffers(s_Device, &cmdAlloc, &uploadCmd);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(uploadCmd, &beginInfo);

        VkImageMemoryBarrier2 toTransfer{};
        toTransfer.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        toTransfer.srcStageMask     = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        toTransfer.srcAccessMask    = 0;
        toTransfer.dstStageMask     = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        toTransfer.dstAccessMask    = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        toTransfer.oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.image            = slot.Image;
        toTransfer.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        VkDependencyInfo dep1{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO, nullptr, 0, 0, nullptr, 0, nullptr, 1, &toTransfer };
        vkCmdPipelineBarrier2(uploadCmd, &dep1);

        VkBufferImageCopy region{};
        region.bufferOffset                    = 0;
        region.bufferRowLength                 = 0;
        region.bufferImageHeight               = 0;
        region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel       = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount     = 1;
        region.imageOffset                     = { 0, 0, 0 };
        region.imageExtent                     = { static_cast<UINT>(tex.Width), static_cast<UINT>(tex.Height), 1 };
        vkCmdCopyBufferToImage(uploadCmd, stagingBuffer, slot.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        VkImageMemoryBarrier2 toShader{};
        toShader.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        toShader.srcStageMask     = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        toShader.srcAccessMask    = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        toShader.dstStageMask     = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        toShader.dstAccessMask    = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
        toShader.oldLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toShader.newLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        toShader.image            = slot.Image;
        toShader.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        VkDependencyInfo dep2{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO, nullptr, 0, 0, nullptr, 0, nullptr, 1, &toShader };
        vkCmdPipelineBarrier2(uploadCmd, &dep2);
        vkEndCommandBuffer(uploadCmd);

        VkCommandBufferSubmitInfo cmdSub{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, nullptr, uploadCmd };
        VkSubmitInfo2 submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2, nullptr, 0, 0, nullptr, 1, &cmdSub, 0, nullptr };
        vkQueueSubmit2(s_GraphicsQueue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(s_GraphicsQueue);

        vkFreeCommandBuffers(s_Device, s_CommandPool, 1, &uploadCmd);
        vkDestroyBuffer(s_Device, stagingBuffer, nullptr);
        vkFreeMemory(s_Device, stagingMemory, nullptr);

        // 4. Create ImageView & DescriptorSet
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = slot.Image;
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        if (vkCreateImageView(s_Device, &viewInfo, nullptr, &slot.View) != VK_SUCCESS) {
            vkDestroyImage(s_Device, slot.Image, nullptr);
            vkFreeMemory(s_Device, slot.Memory, nullptr);
            return VK_NULL_HANDLE;
        }

        VkDescriptorSetAllocateInfo dsAlloc{};
        dsAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsAlloc.descriptorPool     = s_DescriptorPool;
        dsAlloc.descriptorSetCount = 1;
        dsAlloc.pSetLayouts        = &s_DescriptorSetLayout;

        if (vkAllocateDescriptorSets(s_Device, &dsAlloc, &slot.DescriptorSet) != VK_SUCCESS) {
            vkDestroyImageView(s_Device, slot.View, nullptr);
            vkDestroyImage(s_Device, slot.Image, nullptr);
            vkFreeMemory(s_Device, slot.Memory, nullptr);
            return VK_NULL_HANDLE;
        }

        VkDescriptorImageInfo imgInfo{};
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imgInfo.imageView   = slot.View;
        imgInfo.sampler     = s_TextureSampler;

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = slot.DescriptorSet;
        write.dstBinding      = 0;
        write.dstArrayElement = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo      = &imgInfo;
        vkUpdateDescriptorSets(s_Device, 1, &write, 0, nullptr);

        s_TextureCache[tex.TextureID] = slot;
        return slot.DescriptorSet;
    }

    static inline BOOL CreateSwapchain() {
        VkSurfaceCapabilitiesKHR capabilities{};
        if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s_PhysicalDevice, s_Surface, &capabilities) != VK_SUCCESS) {
            return false;
        }

        // If window is minimized (0x0 surface), wait until restored before creating swapchain
        if (capabilities.currentExtent.width == 0 || capabilities.currentExtent.height == 0) {
            Vector2D px = M_Window::Get_PixelSize();
            if (px.x <= 0.0f || px.y <= 0.0f) return false;
        }

        UINT formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(s_PhysicalDevice, s_Surface, &formatCount, nullptr);
        if (formatCount == 0) return false;
        Array<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(s_PhysicalDevice, s_Surface, &formatCount, formats.data());

        UINT modeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(s_PhysicalDevice, s_Surface, &modeCount, nullptr);
        if (modeCount == 0) return false;
        Array<VkPresentModeKHR> modes(modeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(s_PhysicalDevice, s_Surface, &modeCount, modes.data());

        VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(formats);
        VkPresentModeKHR   presentMode   = ChoosePresentMode(modes, s_VSync);

        s_Extent      = ChooseExtent(capabilities);
        s_AspectRatio = (s_Extent.height > 0)
            ? (static_cast<FLOAT>(s_Extent.width) / static_cast<FLOAT>(s_Extent.height))
            : (16.0f / 9.0f);
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

        // Create Hardware Depth Buffer (D32_SFLOAT) matching Swapchain Extent
        // 3. Create independent Depth Buffer per frame-in-flight (Zero GPU hazards/pipeline stalls!)
        for (UINT f = 0; f < MAX_FRAMES_IN_FLIGHT; ++f) {
            VkImageCreateInfo depthImgInfo{};
            depthImgInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            depthImgInfo.imageType     = VK_IMAGE_TYPE_2D;
            depthImgInfo.format        = s_DepthFormat;
            depthImgInfo.extent        = { s_Extent.width, s_Extent.height, 1 };
            depthImgInfo.mipLevels     = 1;
            depthImgInfo.arrayLayers   = 1;
            depthImgInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
            depthImgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
            depthImgInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            depthImgInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
            depthImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            if (vkCreateImage(s_Device, &depthImgInfo, nullptr, &s_DepthImages[f]) != VK_SUCCESS) {
                LOG(Error, LogLevel::Core, "Vulkan: Failed to create Depth Image for frame slot [{}]!", f);
                return false;
            }

            VkMemoryRequirements depthMemReqs{};
            vkGetImageMemoryRequirements(s_Device, s_DepthImages[f], &depthMemReqs);

            VkMemoryAllocateInfo depthAllocInfo{};
            depthAllocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            depthAllocInfo.allocationSize  = depthMemReqs.size;
            depthAllocInfo.memoryTypeIndex = FindMemoryType(depthMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            if (vkAllocateMemory(s_Device, &depthAllocInfo, nullptr, &s_DepthMemories[f]) != VK_SUCCESS ||
                vkBindImageMemory(s_Device, s_DepthImages[f], s_DepthMemories[f], 0) != VK_SUCCESS) {
                LOG(Error, LogLevel::Core, "Vulkan: Failed to allocate/bind Depth Memory for frame slot [{}]!", f);
                return false;
            }

            VkImageViewCreateInfo depthViewInfo{};
            depthViewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            depthViewInfo.image                           = s_DepthImages[f];
            depthViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
            depthViewInfo.format                          = s_DepthFormat;
            depthViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
            depthViewInfo.subresourceRange.baseMipLevel   = 0;
            depthViewInfo.subresourceRange.levelCount     = 1;
            depthViewInfo.subresourceRange.baseArrayLayer = 0;
            depthViewInfo.subresourceRange.layerCount     = 1;

            if (vkCreateImageView(s_Device, &depthViewInfo, nullptr, &s_DepthImageViews[f]) != VK_SUCCESS) {
                LOG(Error, LogLevel::Core, "Vulkan: Failed to create Depth ImageView for frame slot [{}]!", f);
                return false;
            }
        }

        LOG(Info, LogLevel::Core, "Vulkan: Swapchain created ({}x{}, {} images, VSync: {})",
            s_Extent.width, s_Extent.height, actualCount, s_VSync ? "ON" : "OFF");
        return true;
    }

    static inline void CleanupSwapchain() {
        if (!s_Device) return;

        for (UINT f = 0; f < MAX_FRAMES_IN_FLIGHT; ++f) {
            if (s_DepthImageViews[f]) {
                vkDestroyImageView(s_Device, s_DepthImageViews[f], nullptr);
                s_DepthImageViews[f] = VK_NULL_HANDLE;
            }
            if (s_DepthImages[f]) {
                vkDestroyImage(s_Device, s_DepthImages[f], nullptr);
                s_DepthImages[f] = VK_NULL_HANDLE;
            }
            if (s_DepthMemories[f]) {
                vkFreeMemory(s_Device, s_DepthMemories[f], nullptr);
                s_DepthMemories[f] = VK_NULL_HANDLE;
            }
        }

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

    // Initializes Vulkan 1.3/1.4 Device, Surface, Swapchain & Mesh Pipeline
    static inline BOOL Init(SDL_Window *window) {
        if (s_IsInitialized) return true;
        if (!window) {
            LOG(Error, LogLevel::Core, "Vulkan::Init failed: window handle is NULL!");
            return false;
        }

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

        // 12. Create 3D/2D Mesh Pipeline & Per-Frame Buffers
        if (!CreateMeshPipelineAndBuffers()) {
            LOG(Error, LogLevel::Core, "Vulkan: Failed to create Mesh Pipeline!");
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

            DestroyMeshPipelineAndBuffers();

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

        SDL_Vulkan_UnloadLibrary();

        s_IsFrameActive = false;
        s_IsInitialized = false;
        LOG(Info, LogLevel::Core, "Vulkan: Backend shutdown cleanly.");
    }

    // Handles window resizing & minimizing smoothly
    static inline void Recreate() {
        if (!s_Device || M_Window::Is_Minimized()) return;

        Vector2D px = M_Window::Get_PixelSize();
        if (px.x <= 0.0f || px.y <= 0.0f) return;

        VkSurfaceCapabilitiesKHR capabilities{};
        if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s_PhysicalDevice, s_Surface, &capabilities) != VK_SUCCESS) return;
        if (capabilities.currentExtent.width == 0 || capabilities.currentExtent.height == 0) return;

        vkDeviceWaitIdle(s_Device);
        CleanupSwapchain();
        if (CreateSwapchain()) {
            s_NeedsResize = false;
        } else {
            s_NeedsResize = true;
        }
    }

    // ========================================================================
    // Frame Rendering (Begin_Frame -> Draw_Mesh -> End_Frame)
    // ========================================================================

    // Prepares the screen for drawing. Clears screen to s_ClearColor using Dynamic Rendering.
    static inline BOOL Begin_Frame() {
        if (!s_IsInitialized || M_Window::Is_Minimized()) return false;

        if (s_NeedsResize || M_Window::Was_Resized() || s_Swapchain == VK_NULL_HANDLE) {
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

        // Reset dynamic vertex/index buffer cursors to the second half of the buffer
        // (First half [0 .. STATIC_VERTEX_CAPACITY) is preserved across frames for cached Static Meshes!)
        s_FrameVertexOffset = STATIC_VERTEX_CAPACITY;
        s_FrameIndexOffset  = STATIC_INDEX_CAPACITY;

        // 3. Record Commands
        VkCommandBuffer cmd = s_CommandBuffers[s_CurrentFrameIndex];
        vkResetCommandBuffer(cmd, 0);

        VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        vkBeginCommandBuffer(cmd, &beginInfo);

        // 4. Transition Color & Depth Attachments via Sync 2
        VkImageMemoryBarrier2 barriers[2]{};
        barriers[0].sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barriers[0].srcStageMask     = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barriers[0].srcAccessMask    = 0;
        barriers[0].dstStageMask     = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barriers[0].dstAccessMask    = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        barriers[0].oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
        barriers[0].newLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barriers[0].image            = s_Images[s_CurrentImageIndex];
        barriers[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        barriers[1].sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barriers[1].srcStageMask     = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        barriers[1].srcAccessMask    = 0;
        barriers[1].dstStageMask     = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        barriers[1].dstAccessMask    = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barriers[1].oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
        barriers[1].newLayout        = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        barriers[1].image            = s_DepthImages[s_CurrentFrameIndex];
        barriers[1].subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };

        VkDependencyInfo depInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO, nullptr, 0, 0, nullptr, 0, nullptr, 2, barriers };
        vkCmdPipelineBarrier2(cmd, &depInfo);

        // 5. Dynamic Rendering: vkCmdBeginRendering (Color + Depth Attachments)
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

        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType                   = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView               = s_DepthImageViews[s_CurrentFrameIndex];
        depthAttachment.imageLayout             = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp                 = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.clearValue.depthStencil = { 1.0f, 0 };

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea           = { { 0, 0 }, s_Extent };
        renderingInfo.layerCount           = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments    = &colorAttachment;
        renderingInfo.pDepthAttachment     = &depthAttachment;

        vkCmdBeginRendering(cmd, &renderingInfo);

        VkViewport viewport{ 0.0f, 0.0f, static_cast<FLOAT>(s_Extent.width), static_cast<FLOAT>(s_Extent.height), 0.0f, 1.0f };
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        VkRect2D scissor{ { 0, 0 }, s_Extent };
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // Bind Mesh Pipeline & Frame Buffers
        if (s_Pipeline) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s_Pipeline);
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &s_VertexBuffers[s_CurrentFrameIndex], &offset);
            vkCmdBindIndexBuffer(cmd, s_IndexBuffers[s_CurrentFrameIndex], 0, VK_INDEX_TYPE_UINT32);
        }

        s_LastBoundTextureID = 0xFFFFFFFFu;
        s_IsFrameActive = true;
        return true;
    }

    // Draws a 3D/2D Mesh with transform, color tint, alpha transparency, and optional TextureID.
    // Automatically caches static meshes & textures in GPU memory so unchanged assets are NEVER re-uploaded!
    static inline void Draw_Mesh(
        const MeshData &mesh,
        const Vector3D &pos,
        const Vector3D &rot,
        const Vector3D &size,
        const COLOR    &color,
        FLOAT           transparency,
        UINT            textureID = 0)
    {
        if (!s_IsFrameActive || !s_Pipeline || !mesh.Is_Valid() || transparency <= 0.001f) return;

        const UINT vCount   = mesh.Get_VertexCount();
        const UINT iCount   = mesh.Get_IndexCount();
        const UINT frameIdx = s_CurrentFrameIndex;
        VkCommandBuffer cmd = s_CommandBuffers[frameIdx];

        if (textureID != s_LastBoundTextureID) {
            VkDescriptorSet texSet = GetOrUploadTextureSet(textureID);
            if (texSet != VK_NULL_HANDLE) {
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s_PipelineLayout, 0, 1, &texSet, 0, nullptr);
                s_LastBoundTextureID = textureID;
            }
        }

        UINT drawVertexOffset = 0;
        UINT drawIndexOffset  = 0;
        BOOL bUsedStaticCache = false;

        if (mesh.MeshID != 0) {
            const COLOR &c0 = mesh.Vertices[0].Color;
            const DWORD firstColor = (DWORD(c0.r) << 24) | (DWORD(c0.g) << 16) | (DWORD(c0.b) << 8) | DWORD(c0.a);

            auto &cache = s_MeshCache[frameIdx];
            auto  it    = cache.find(mesh.MeshID);

            if (it != cache.end()) {
                const CachedMeshSlot &slot = it->second;
                if (slot.Revision    == mesh.Revision &&
                    slot.FirstColor  == firstColor    &&
                    slot.VertexCount == vCount        &&
                    slot.IndexCount  == iCount)
                {
                    // Zero-copy GPU Static Cache Hit! No vertex/index memcpy needed!
                    drawVertexOffset = slot.VertexOffset;
                    drawIndexOffset  = slot.IndexOffset;
                    bUsedStaticCache = true;
                }
            } else if (s_StaticVertexCursor[frameIdx] + vCount <= STATIC_VERTEX_CAPACITY &&
                       s_StaticIndexCursor[frameIdx]  + iCount <= STATIC_INDEX_CAPACITY)
            {
                // First time seeing this MeshID in this frame slot -> upload once into Static Cache region
                drawVertexOffset = s_StaticVertexCursor[frameIdx];
                drawIndexOffset  = s_StaticIndexCursor[frameIdx];

                std::memcpy(s_MappedVertices[frameIdx] + drawVertexOffset, mesh.Vertices.data(), vCount * sizeof(Vertex));
                std::memcpy(s_MappedIndices[frameIdx]  + drawIndexOffset,  mesh.Indices.data(),  iCount * sizeof(UINT));

                s_StaticVertexCursor[frameIdx] += vCount;
                s_StaticIndexCursor[frameIdx]  += iCount;

                cache[mesh.MeshID] = { drawVertexOffset, drawIndexOffset, vCount, iCount, mesh.Revision, firstColor };
                bUsedStaticCache   = true;
            }
        }

        // Dynamic Fallback (for meshes whose vertices mutate every frame, e.g. animated vertex colors)
        if (!bUsedStaticCache) {
            if (s_FrameVertexOffset + vCount > MAX_VERTICES_PER_FRAME ||
                s_FrameIndexOffset  + iCount > MAX_INDICES_PER_FRAME) {
                return;
            }

            drawVertexOffset = s_FrameVertexOffset;
            drawIndexOffset  = s_FrameIndexOffset;

            std::memcpy(s_MappedVertices[frameIdx] + drawVertexOffset, mesh.Vertices.data(), vCount * sizeof(Vertex));
            std::memcpy(s_MappedIndices[frameIdx]  + drawIndexOffset,  mesh.Indices.data(),  iCount * sizeof(UINT));

            s_FrameVertexOffset += vCount;
            s_FrameIndexOffset  += iCount;
        }

        // Enable 3D directional face shading on 3D shapes (> 4 vertices, e.g. Cube), keep 2D triangles pure unlit RGB
        BOOL bShade3D = (vCount > 4);
        DrawConstants dc = M_Mesh::Build_DrawConstants(pos, rot, size, color, transparency, s_AspectRatio, true, bShade3D);

        vkCmdPushConstants(cmd, s_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(DrawConstants), &dc);
        vkCmdDrawIndexed(cmd, iCount, 1, drawIndexOffset, static_cast<INT>(drawVertexOffset), 0);
    }

    // Finishes rendering, transitions image to PRESENT layout and presents to screen
    static inline void End_Frame() {
        if (!s_IsInitialized || !s_IsFrameActive) return;
        s_IsFrameActive = false;

        VkCommandBuffer cmd = s_CommandBuffers[s_CurrentFrameIndex];
        vkCmdEndRendering(cmd);

        // Transition: COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
        VkImageMemoryBarrier2 barrier{};
        barrier.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcStageMask     = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.srcAccessMask    = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstStageMask     = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        barrier.dstAccessMask    = 0;
        barrier.oldLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout        = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.image            = s_Images[s_CurrentImageIndex];
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
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || M_Window::Was_Resized()) {
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
        s_NeedsResize = true;
    }

    static inline BOOL       Get_VSync()                     { return s_VSync; }
    static inline void       Set_ClearColor(const COLOR &c)  { s_ClearColor = c; }
    static inline COLOR      Get_ClearColor()                { return s_ClearColor; }
    static inline StringView Get_GPUName()                   { return s_GPUName; }
    static inline BOOL       Is_Initialized()                { return s_IsInitialized; }
    static inline VkCommandBuffer Get_CurrentCommandBuffer() { return s_CommandBuffers[s_CurrentFrameIndex]; }
};
