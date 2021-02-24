//
// Created by Kevin on 07/02/2021.
//

#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <numeric>
#include <glm/ext/matrix_float4x4.hpp>
#include "Utils.h"

struct DeviceContext{
    VkPhysicalDevice& pdevice;
    VkDevice& device;

    VkQueue graphics;
    uint32_t graphics_index;
};

struct Buffer {
    uint32_t size;
    uint32_t offset;
};

struct GeometryBuffer {
    uint32_t n_of_indices;
    uint32_t n_of_vertices;

    uint32_t indices_size;
    uint32_t vertices_size;

    uint32_t indices_offset;
    uint32_t vertices_offset;

    VkBuffer buffer;
    VkDeviceMemory memory;

    uint32_t size() const {
        return indices_size + vertices_size;
    }
};

struct Image{
    VkImage image;
    VkImageView imageview;
    VkDeviceMemory imagememory;

    VkSampler sampler;
};

enum UniformType{
    TYPE_BUFFER,
    TYPE_IMAGE,
    TYPE_CUBEMAP
};
struct Uniform {
    UniformType type;
    std::array<uint32_t, 3> size;
    uint32_t byte_size;

    uint32_t count{1};

    std::shared_ptr<void> data;
};

struct UniformSet{
    uint32_t slot{0};
    std::map<uint32_t, Uniform> uniforms;
};

struct DescriptorLayout{
    VkDescriptorSetLayout layout;
};

struct DescriptorPool{
    VkDescriptorPool pool;
};

struct DescriptorSet{
    VkDescriptorSet set;

    std::map<uint32_t, Uniform> uniforms;

    VkBuffer uniform_buffer;
    VkDeviceMemory uniform_memory;

    std::map<uint32_t, Buffer> buffersForSlot;
    std::map<uint32_t, Image> imagesForSlot;
};

struct Pipeline {
    VkShaderModule vertex_module{0};
    VkShaderModule fragment_module{0};

    VkPipeline pipeline{0};
    VkPipelineLayout pipeline_layout{0};

    VkExtent2D extent{0, 0};
};

struct RenderObject {
    ObjectNode* node;
    GeometryBuffer geometry;
    std::map<uint32_t, DescriptorSet> descriptors;
    Pipeline pipeline;
};

struct LightObject{
    LightNode* node;
    Image shadow_map;
    VkFramebuffer framebuffer;
};
