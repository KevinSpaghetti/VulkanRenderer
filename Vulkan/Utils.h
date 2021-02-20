//
// Created by Kevin on 23/01/2021.
//

#pragma once

#include <string>
#include <vulkan/vulkan_core.h>
#include <sstream>
#include <vector>
#include "../SceneGraph/Geometry.h"

struct SurfaceParams{
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR format;
    VkPresentModeKHR present_mode;
};

class Utils {
public:
    static std::string getReadableQueueFlags(const VkQueueFlags bitfield) {
        std::stringstream s;
        const std::vector<std::pair<VkQueueFlagBits, std::string>> flags{
                {VkQueueFlagBits::VK_QUEUE_GRAPHICS_BIT,       "GRAPHICS"},
                {VkQueueFlagBits::VK_QUEUE_COMPUTE_BIT,        "COMPUTE"},
                {VkQueueFlagBits::VK_QUEUE_TRANSFER_BIT,       "TRANSFER"},
                {VkQueueFlagBits::VK_QUEUE_PROTECTED_BIT,      "PROTECTED"},
                {VkQueueFlagBits::VK_QUEUE_SPARSE_BINDING_BIT, "SPARSE_BINDING"}
        };

        for (auto[flag, name] : flags) {
            if (flag & bitfield) {
                s << name << " ";
            }
        }

        return s.str();

    }

    static uint32_t indexOfQueueFamilyWithFlags(const std::vector<VkQueueFamilyProperties> &queue_family_props,
                                                const VkQueueFlags flags, const VkQueueFlags avoid = 0) {
        int index = 0;
        for (auto props : queue_family_props) {
            if ((props.queueFlags & flags) == flags) {
                return index;
            }
            ++index;
        }
        return -1;
    }

    static SurfaceParams chooseSurfaceParams(const VkPhysicalDevice &pdevice,
                                             const VkSurfaceKHR &surface,
                                             const VkSurfaceFormatKHR desiredFormat,
                                             const VkPresentModeKHR desiredPresentMode) {

        SurfaceParams surfaceParams{};

        //Query m_surface capabilities
        VkSurfaceCapabilitiesKHR surface_capabilities{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pdevice, surface, &surfaceParams.capabilities);

        //Query m_surface and present mode supported formats
        uint32_t formatcount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(pdevice, surface, &formatcount, nullptr);
        std::vector<VkSurfaceFormatKHR> supported_surface_formats(formatcount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(pdevice, surface, &formatcount, supported_surface_formats.data());
        uint32_t presentation_mode_count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(pdevice, surface, &presentation_mode_count, nullptr);
        std::vector<VkPresentModeKHR> supported_present_modes(presentation_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(pdevice, surface, &presentation_mode_count,
                                                  supported_present_modes.data());

        //Pick the desired format and presentation modes
        surfaceParams.format = supported_surface_formats.front();
        surfaceParams.present_mode = supported_present_modes.front();

        for (auto format : supported_surface_formats) {
            if (format.format == desiredFormat.format && format.colorSpace == desiredFormat.colorSpace) {
                surfaceParams.format = format;
            }
        }
        for (auto mode : supported_present_modes) {
            if (mode == desiredPresentMode) {
                surfaceParams.present_mode = mode;
            }
        }

        return surfaceParams;
    }

    static VkFormat findDepthFormat(const VkPhysicalDevice pdevice) {
        return findSupportedFormat(
                pdevice,
                {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    static bool hasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    static VkFormat findSupportedFormat(const VkPhysicalDevice pdevice,
                                        const std::vector<VkFormat> &candidates,
                                        const VkImageTiling tiling,
                                        const VkFormatFeatureFlags features) {

        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(pdevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        throw std::runtime_error("failed to find supported format!");
    }

    static void createImage(const VkPhysicalDevice pdevice, const VkDevice device,
                            VkImage &image,
                            VkDeviceMemory &imageMemory,
                            const VkExtent2D extent,
                            const VkFormat format,
                            const VkImageTiling tiling,
                            const VkImageUsageFlags usage,
                            const VkMemoryPropertyFlags properties) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = extent.width;
        imageInfo.extent.height = extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image, &memRequirements);

        int memory_index = findMemoryTypeIndexForImage(pdevice, device, image, properties);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = memory_index;

        if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }

        vkBindImageMemory(device, image, imageMemory, 0);
    }

    static void createImageView(const VkDevice device,
                                VkImageView &image_view,
                                const VkImage image,
                                const VkFormat format,
                                const VkImageAspectFlags aspect) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspect;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &image_view) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture image view!");
        }

    }

    static void createCubemap(const VkPhysicalDevice pdevice, const VkDevice device,
                              VkImage &image,
                              VkImageView &image_view,
                              VkDeviceMemory &imageMemory,
                              const VkExtent2D extent,
                              const VkFormat format,
                              const VkImageTiling tiling,
                              const VkImageUsageFlags usage,
                              const VkImageAspectFlags aspect,
                              const VkMemoryPropertyFlags properties) {

        constexpr uint32_t cubemap_sides = 6;

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = extent.width;
        imageInfo.extent.height = extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = cubemap_sides;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

        if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image, &memRequirements);
        int memory_index = findMemoryTypeIndexForImage(pdevice, device, image, properties);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = memory_index;
        if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }

        vkBindImageMemory(device, image, imageMemory, 0);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspect;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = cubemap_sides;

        if (vkCreateImageView(device, &viewInfo, nullptr, &image_view) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture image view!");
        }

    }

    static void createBuffer(const VkDevice &device,
                             VkBuffer &buffer,
                             VkBufferUsageFlags usage,
                             VkSharingMode sharingMode,
                             const uint32_t size) {
        VkBufferCreateInfo indices_cinfo{};
        indices_cinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        indices_cinfo.size = size;
        indices_cinfo.usage = usage;
        indices_cinfo.sharingMode = sharingMode;

        if (vkCreateBuffer(device, &indices_cinfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create vertex buffer");
        }
    }

    static uint32_t allocateDeviceMemory(const VkPhysicalDevice &pdevice,
                                         const VkDevice &device,
                                         const VkBuffer buffer,
                                         VkDeviceMemory &memory,
                                         const uint32_t nOfBytes,
                                         const VkMemoryPropertyFlags memory_flags) {

        uint32_t result_size = 0;

        int memory_type_index = findMemoryTypeIndexForBuffer(pdevice,
                                                             device,
                                                             buffer,
                                                             memory_flags,
                                                             result_size);

        VkMemoryAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = result_size;
        alloc_info.memoryTypeIndex = memory_type_index;

        if (vkAllocateMemory(device, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create vertex buffer");
        }

        return result_size;
    }

    static int findMemoryTypeIndexForBuffer(const VkPhysicalDevice pdevice,
                                            const VkDevice device,
                                            const VkBuffer &buffer,
                                            const VkMemoryPropertyFlags flags,
                                            uint32_t &nOfBytes) {

        VkMemoryRequirements reqs;
        vkGetBufferMemoryRequirements(device, buffer, &reqs);

        nOfBytes = reqs.size;

        VkPhysicalDeviceMemoryProperties memory_props;
        vkGetPhysicalDeviceMemoryProperties(pdevice, &memory_props);

        int memory_type_index = 0;
        for (uint32_t i = 0; i < memory_props.memoryTypeCount; ++i) {
            if (reqs.memoryTypeBits & (1 << i) &&
                (memory_props.memoryTypes[i].propertyFlags & flags) == flags) {
                memory_type_index = i;
            }
        }

        return memory_type_index;
    }

    static int findMemoryTypeIndexForImage(const VkPhysicalDevice pdevice,
                                           const VkDevice device,
                                           const VkImage &image,
                                           const VkMemoryPropertyFlags flags) {

        VkMemoryRequirements reqs;
        vkGetImageMemoryRequirements(device, image, &reqs);

        VkPhysicalDeviceMemoryProperties memory_props;
        vkGetPhysicalDeviceMemoryProperties(pdevice, &memory_props);

        int memory_type_index = 0;
        for (uint32_t i = 0; i < memory_props.memoryTypeCount; ++i) {
            if (reqs.memoryTypeBits & (1 << i) &&
                (memory_props.memoryTypes[i].propertyFlags & flags) == flags) {
                memory_type_index = i;
            }
        }

        return memory_type_index;
    }

    static void copyToMemory(const VkDevice device,
                             const VkDeviceMemory &memory,
                             const void *data,
                             const uint32_t nOfBytes,
                             const uint32_t offset) {
        void *shared_memory_begin;
        vkMapMemory(device, memory, offset, nOfBytes, 0, &shared_memory_begin);
        memcpy(shared_memory_begin, data, nOfBytes);
        vkUnmapMemory(device, memory);
    }

    static void createShaderModule(const VkDevice device,
                                   VkShaderModule &module,
                                   const std::vector<char> shader_code) {
        VkShaderModuleCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = shader_code.size();
        info.pCode = reinterpret_cast<const uint32_t*>(shader_code.data());

        if (vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module");
        }
    }


    static void createPipeline(const VkDevice device,
                               VkPipeline &pipeline,
                               VkPipelineLayout &pipeline_layout,
                               const std::vector<VkDescriptorSetLayout> &descriptorSetsLayouts,
                               const VkRenderPass render_pass,
                               const VkShaderModule vertex_shader,
                               const VkShaderModule fragment_shader,
                               const VkExtent2D extent) {


        VkPipelineShaderStageCreateInfo vertexinfo{};
        vertexinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertexinfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexinfo.module = vertex_shader;
        vertexinfo.pName = "main";
        VkPipelineShaderStageCreateInfo fragmentinfo{};
        fragmentinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragmentinfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragmentinfo.module = fragment_shader;
        fragmentinfo.pName = "main";

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) extent.width;
        viewport.height = (float) extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = extent;

        VkPipelineViewportStateCreateInfo vs{};
        vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vs.viewportCount = 1;
        vs.pViewports = &viewport;
        vs.scissorCount = 1;
        vs.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f;
        rasterizer.depthBiasClamp = 0.0f;
        rasterizer.depthBiasSlopeFactor = 0.0f;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f;
        multisampling.pSampleMask = nullptr;
        multisampling.alphaToCoverageEnable = VK_FALSE;
        multisampling.alphaToOneEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional


        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = descriptorSetsLayouts.size();
        pipelineLayoutInfo.pSetLayouts = descriptorSetsLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipeline_layout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        VkVertexInputBindingDescription bdesc{};
        bdesc.binding = 0;
        bdesc.stride = sizeof(VertexData);
        bdesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription pos_attr{};
        pos_attr.binding = 0;
        pos_attr.location = 0;
        pos_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
        pos_attr.offset = offsetof(VertexData, position);

        VkVertexInputAttributeDescription nor_attr{};
        nor_attr.binding = 0;
        nor_attr.location = 1;
        nor_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
        nor_attr.offset = offsetof(VertexData, normal_1);

        VkVertexInputAttributeDescription col_attr{};
        col_attr.binding = 0;
        col_attr.location = 2;
        col_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
        col_attr.offset = offsetof(VertexData, color_1);

        VkVertexInputAttributeDescription texcoord_1{};
        texcoord_1.binding = 0;
        texcoord_1.location = 3;
        texcoord_1.format = VK_FORMAT_R32G32_SFLOAT;
        texcoord_1.offset = offsetof(VertexData, texcoord_1);

        VkVertexInputAttributeDescription texcoord_2{};
        texcoord_2.binding = 0;
        texcoord_2.location = 4;
        texcoord_2.format = VK_FORMAT_R32G32_SFLOAT;
        texcoord_2.offset = offsetof(VertexData, texcoord_2);

        std::vector<VkVertexInputAttributeDescription> descs = {pos_attr, nor_attr, col_attr, texcoord_1, texcoord_2};

        VkPipelineVertexInputStateCreateInfo vinfo{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vinfo.vertexBindingDescriptionCount = 1;
        vinfo.pVertexBindingDescriptions = &bdesc;
        vinfo.vertexAttributeDescriptionCount = descs.size();
        vinfo.pVertexAttributeDescriptions = descs.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{
                VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f;
        depthStencil.maxDepthBounds = 1.0f;
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {};
        depthStencil.back = {};

        VkGraphicsPipelineCreateInfo pipelineinfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineinfo.stageCount = 2;
        std::vector<VkPipelineShaderStageCreateInfo> modules({vertexinfo, fragmentinfo});
        pipelineinfo.pStages = modules.data();
        pipelineinfo.pVertexInputState = &vinfo;
        pipelineinfo.pInputAssemblyState = &inputAssembly;
        pipelineinfo.pViewportState = &vs;
        pipelineinfo.pRasterizationState = &rasterizer;
        pipelineinfo.pMultisampleState = &multisampling;
        pipelineinfo.pDepthStencilState = nullptr;
        pipelineinfo.pColorBlendState = &colorBlending;
        pipelineinfo.pDynamicState = nullptr;
        pipelineinfo.layout = pipeline_layout;
        pipelineinfo.renderPass = render_pass;
        pipelineinfo.subpass = 0;
        pipelineinfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineinfo.basePipelineIndex = -1;
        pipelineinfo.pDepthStencilState = &depthStencil;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineinfo, nullptr, &pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Pipeline error");
        }
    }

};