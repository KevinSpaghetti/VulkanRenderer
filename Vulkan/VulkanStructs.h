//
// Created by Kevin on 28/01/2021.
//

#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <numeric>
#include <glm/ext/matrix_float4x4.hpp>
#include "Utils.h"
#include "Resources.h"

GeometryBuffer createBuffer(const DeviceContext& context,
                            const Geometry& geometry){
    GeometryBuffer buffer{};
    buffer.n_of_indices = geometry.indices().size();
    buffer.n_of_vertices = geometry.vertices().size();
    buffer.indices_size = geometry.indices().size() * sizeof(uint32_t);
    buffer.vertices_size = geometry.vertices().size() * sizeof(VertexData);
    buffer.indices_offset = 0;
    buffer.vertices_offset = buffer.indices_size;

    Utils::createBuffer(context.device,
                        buffer.buffer,
                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                        VK_SHARING_MODE_EXCLUSIVE,
                        buffer.size());
    Utils::allocateDeviceMemory(context.pdevice, context.device,
                                buffer.buffer,
                                buffer.memory,
                                buffer.size(),
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    vkBindBufferMemory(context.device, buffer.buffer, buffer.memory, 0);

    Utils::copyToMemory(context.device, buffer.memory, geometry.indices().data(),
            buffer.indices_size, buffer.indices_offset);
    Utils::copyToMemory(context.device, buffer.memory, geometry.vertices().data(),
            buffer.vertices_size, buffer.vertices_offset);


    return buffer;
}
std::vector<GeometryBuffer> createBuffers(const DeviceContext& context,
                                          const std::vector<Geometry>& geometries){
    std::vector<GeometryBuffer> objects(geometries.size());

    for (int i = 0; i < objects.size(); ++i) {
        objects[i] = createBuffer(context, geometries[i]);
    }

    return objects;
}

Image createImage(const DeviceContext& context, glm::ivec2 size){
    Image result{};

    Utils::createImage(context.pdevice, context.device,
                       result.image,
                       result.imagememory,
                       VkExtent2D{
                               static_cast<uint32_t>(size.x),
                               static_cast<uint32_t>(size.y)},
                       VK_FORMAT_R8G8B8A8_SRGB,
                       VK_IMAGE_TILING_OPTIMAL,
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    Utils::createImageView(context.device,
                           result.imageview,
                           result.image,
                           VK_FORMAT_R8G8B8A8_SRGB,
                           VK_IMAGE_ASPECT_COLOR_BIT);

    VkSamplerCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = VK_FILTER_LINEAR;
    info.minFilter = VK_FILTER_LINEAR;
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    info.anisotropyEnable = VK_FALSE;
    info.maxAnisotropy = 1.0f;
    info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    info.unnormalizedCoordinates = VK_FALSE;
    info.compareEnable = VK_FALSE;
    info.compareOp = VK_COMPARE_OP_ALWAYS;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.mipLodBias = 0.0f;
    info.minLod = 0.0f;
    info.maxLod = 0.0f;
    vkCreateSampler(context.device, &info, nullptr, &result.sampler);

    return result;
}
void uploadImageData(const DeviceContext& context,
        const std::vector<Image>& images,
        const std::vector<void*>& textures,
        const std::vector<std::array<uint32_t, 3>>& image_sizes,
        const std::vector<uint32_t>& byte_syzes){

    VkCommandPool createResourcesPool{};
    VkCommandBuffer command;

    VkCommandPoolCreateInfo poolinfo{};
    poolinfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolinfo.queueFamilyIndex = context.graphics_index;
    poolinfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(context.device, &poolinfo, nullptr, &createResourcesPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }

    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = createResourcesPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(context.device, &commandBufferAllocateInfo, &command) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }

    VkMemoryPropertyFlags flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    std::vector<VkBuffer> stagingBuffers(images.size());
    std::vector<VkDeviceMemory> stagingBuffersMemory(images.size());

    for(int i = 0; i < images.size(); ++i) {
        const auto &image = images[i];
        const auto &texture = textures[i];
        const auto &image_size = image_sizes[i];
        const auto &data_size = byte_syzes[i];

        unsigned char c;
        for (int i = 0; i < data_size; ++i) {
            c = ((unsigned char*)texture)[i];
        }

        uint32_t alloc_size = 0;
        Utils::createBuffer(context.device, stagingBuffers[i], flags, VK_SHARING_MODE_EXCLUSIVE, data_size);
        alloc_size = Utils::allocateDeviceMemory(context.pdevice, context.device,
                                    stagingBuffers[i],
                                    stagingBuffersMemory[i],
                                    data_size,
                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        Utils::copyToMemory(context.device, stagingBuffersMemory[i], texture, data_size, 0);
        vkBindBufferMemory(context.device, stagingBuffers[i], stagingBuffersMemory[i], 0);
    }

    //Register and submit in one pass all the transfer commands
    VkCommandBufferBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(command, &info);

    for(int i = 0; i < images.size(); ++i) {
        const auto &image = images[i];
        const auto &texture = textures[i];
        const auto &stagingBuffer = stagingBuffers[i];
        const auto &image_size = image_sizes[i];
        const auto &data_size = byte_syzes[i];

        //Layout transitions
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image.image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(command,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {
                static_cast<uint32_t>(image_size[0]),
                static_cast<uint32_t>(image_size[1]),
                1
        };

        vkCmdCopyBufferToImage(command,
                               stagingBuffer,
                               image.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1, &region);

        VkImageMemoryBarrier barrier2{};
        barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier2.image = image.image;
        barrier2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier2.subresourceRange.baseMipLevel = 0;
        barrier2.subresourceRange.levelCount = 1;
        barrier2.subresourceRange.baseArrayLayer = 0;
        barrier2.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(command,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier2);

    }

    vkEndCommandBuffer(command);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &command;
    vkQueueSubmit(context.graphics, 1, &submitInfo, VK_NULL_HANDLE);

    vkQueueWaitIdle(context.graphics);

    std::for_each(stagingBuffers.begin(), stagingBuffers.end(),
            [&context](const auto& buffer){ vkDestroyBuffer(context.device, buffer, nullptr);});
    std::for_each(stagingBuffersMemory.begin(), stagingBuffersMemory.end(),
            [&context](const auto& memory){ vkFreeMemory(context.device, memory, nullptr);});

    vkDestroyCommandPool(context.device, createResourcesPool, nullptr);
}

/*
Image createCubemap(const DeviceContext& context, glm::ivec2 size){
    Image result{};

    Utils::createCubemap(context.pdevice, context.device,
                       result.image,
                       result.imageview,
                       result.imagememory,
                       VkExtent2D{
                               static_cast<uint32_t>(size.x),
                               static_cast<uint32_t>(size.y)},
                       VK_FORMAT_R8G8B8A8_SRGB,
                       VK_IMAGE_TILING_OPTIMAL,
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                       VK_IMAGE_ASPECT_COLOR_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkSamplerCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = VK_FILTER_LINEAR;
    info.minFilter = VK_FILTER_LINEAR;
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;

    info.anisotropyEnable = VK_FALSE;
    info.maxAnisotropy = 1.0f;
    info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    info.unnormalizedCoordinates = VK_FALSE;
    info.compareEnable = VK_FALSE;
    info.compareOp = VK_COMPARE_OP_ALWAYS;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.mipLodBias = 0.0f;
    info.minLod = 0.0f;
    info.maxLod = 0.0f;
    vkCreateSampler(context.device, &info, nullptr, &result.sampler);

    return result;
}
void uploadCubemapData(const DeviceContext& context,
                     Image& cubemap,
                     const std::vector<Texture2D>& textures){

    VkCommandPool createResourcesPool{};
    VkCommandBuffer command;

    VkCommandPoolCreateInfo poolinfo{};
    poolinfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolinfo.queueFamilyIndex = context.graphics_index;
    poolinfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(context.device, &poolinfo, nullptr, &createResourcesPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }

    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = createResourcesPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(context.device, &commandBufferAllocateInfo, &command) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }

    VkMemoryPropertyFlags flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    std::vector<VkBuffer> stagingBuffers(textures.size());
    std::vector<VkDeviceMemory> stagingBuffersMemory(textures.size());

    for(int i = 0; i < textures.size(); ++i) {
        const auto &image = textures[i];
        const auto &texture = textures[i];

        Utils::createBuffer(context.device, stagingBuffers[i], flags, VK_SHARING_MODE_EXCLUSIVE, texture.data_size());
        Utils::allocateDeviceMemory(context.pdevice, context.device,
                                    stagingBuffers[i],
                                    stagingBuffersMemory[i],
                                    texture.data_size(),
                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        Utils::copyToMemory(context.device, stagingBuffersMemory[i], texture.data().data(), texture.data_size(), 0);
        vkBindBufferMemory(context.device, stagingBuffers[i], stagingBuffersMemory[i], 0);
    }

    //Register and submit in one pass all the transfer commands
    VkCommandBufferBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(command, &info);

    for(int i = 0; i < textures.size(); ++i) {
        const auto &image = textures[i];
        const auto &texture = textures[i];
        const auto &stagingBuffer = stagingBuffers[i];

        //Layout transitions
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = cubemap.image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = i;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(command,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = i;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {
                static_cast<uint32_t>(texture.size().x),
                static_cast<uint32_t>(texture.size().y),
                1
        };

        vkCmdCopyBufferToImage(command,
                               stagingBuffer,
                               cubemap.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1, &region);

        VkImageMemoryBarrier barrier2{};
        barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier2.image = cubemap.image;
        barrier2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier2.subresourceRange.baseMipLevel = 0;
        barrier2.subresourceRange.levelCount = 1;
        barrier2.subresourceRange.baseArrayLayer = i;
        barrier2.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(command,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier2);

    }

    vkEndCommandBuffer(command);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &command;
    vkQueueSubmit(context.graphics, 1, &submitInfo, VK_NULL_HANDLE);

    vkQueueWaitIdle(context.graphics);

    std::for_each(stagingBuffers.begin(), stagingBuffers.end(),
                  [&context](const auto& buffer){ vkDestroyBuffer(context.device, buffer, nullptr);});
    std::for_each(stagingBuffersMemory.begin(), stagingBuffersMemory.end(),
                  [&context](const auto& memory){ vkFreeMemory(context.device, memory, nullptr);});

    vkDestroyCommandPool(context.device, createResourcesPool, nullptr);
}
*/

DescriptorPool createDescriptorPool(const DeviceContext& context,
        const std::vector<std::vector<UniformSet>>& sets) {

    DescriptorPool result{};

    uint32_t uniform_buffers_count = 0;
    uint32_t image_samplers_count = 0;

    std::for_each(sets.begin(), sets.end(), [&](const auto& set){
        for(const auto& uniformSet : set){
            for(const auto& [key, uniform] : uniformSet.uniforms){
                if(uniform.type == TYPE_BUFFER){
                    ++uniform_buffers_count;
                }
                if(uniform.type == TYPE_IMAGE || uniform.type == TYPE_CUBEMAP){
                    ++image_samplers_count;
                }
            }
        }
    });

    std::vector<VkDescriptorPoolSize> sizes;
    if(uniform_buffers_count > 0){
        auto& size = sizes.emplace_back();
        size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        size.descriptorCount = uniform_buffers_count;
    }
    if(image_samplers_count > 0){
        auto& size = sizes.emplace_back();
        size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        size.descriptorCount = image_samplers_count;
    }

    VkDescriptorPoolCreateInfo pInfo{};
    pInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pInfo.poolSizeCount = sizes.size();
    pInfo.pPoolSizes = sizes.data();
    pInfo.maxSets = 64;

    if (vkCreateDescriptorPool(context.device, &pInfo, nullptr, &result.pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vertex buffer");
    }

    return result;
}

DescriptorLayout createDescriptorLayout(const DeviceContext& context,
        const UniformSet& set){

    DescriptorLayout layout{};

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    for(const auto& [slot, uniform] : set.uniforms){
        auto& binding = bindings.emplace_back();
        binding.binding = slot;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_ALL;

        if(uniform.type == TYPE_BUFFER){
            binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
        if(uniform.type == TYPE_IMAGE || uniform.type == TYPE_CUBEMAP ){
            binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        }
    }

    VkDescriptorSetLayoutCreateInfo lInfo{};
    lInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    lInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    lInfo.pBindings = bindings.data();
    if (vkCreateDescriptorSetLayout(context.device, &lInfo, nullptr, &layout.layout)) {
        throw std::runtime_error("Failed to create shader module");
    }

    return layout;
}
std::vector<std::vector<DescriptorLayout>> createDescriptorLayouts(const DeviceContext& context,
        const std::vector<std::vector<UniformSet>>& sets){

    std::vector<std::vector<DescriptorLayout>> descLayouts(sets.size());
    for (int i = 0; i < descLayouts.size(); ++i) {
        for (const auto& set : sets[i]) {
            descLayouts[i].push_back(createDescriptorLayout(context, set));
        }
    }

    return descLayouts;
}
std::vector<std::vector<DescriptorSet>> createDescriptorSets(const DeviceContext& context,
        const std::vector<std::vector<UniformSet>>& sets,
        const std::vector<std::vector<DescriptorLayout>>& layouts,
        const DescriptorPool pool){

    std::vector<std::vector<DescriptorSet>> descSets(sets.size());

    for (int i = 0; i < sets.size(); ++i) {
        const auto& uset = sets[i];
        const auto& desc_layouts = layouts[i];
        for (int j = 0; j < uset.size(); ++j) {
            if(!uset[j].uniforms.empty()){
                const auto& uniform_set = uset[j];
                const auto& descriptor_layout = desc_layouts[j];
                DescriptorSet d;
                VkDescriptorSetAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                allocInfo.descriptorPool = pool.pool;
                allocInfo.descriptorSetCount = 1;
                allocInfo.pSetLayouts = &descriptor_layout.layout;
                if (vkAllocateDescriptorSets(context.device, &allocInfo, &d.set) != VK_SUCCESS) {
                    throw std::runtime_error("Failed to create descriptor sets");
                }
                d.slot = uset[j].slot;
                d.uniforms = uset[j].uniforms;
                descSets[i].push_back(d);
            }

        }
    }

    return descSets;
}

//Init the buffers and the images for the descriptor sets
void initDescriptorSets(const DeviceContext& context,
        std::vector<std::vector<DescriptorSet>>& descriptors){

    for(int i = 0; i < descriptors.size(); ++i){
        for(int j = 0; j < descriptors[i].size(); ++j){
            auto& descriptor = descriptors[i][j];

            uint32_t total_uniform_size = std::accumulate(descriptor.uniforms.begin(), descriptor.uniforms.end(), 0,[]
            (const auto &acc, const auto& uniform) {
                return acc + ((uniform.second.type == TYPE_BUFFER) ? uniform.second.byte_size * uniform.second.count : 0);
            });

            uint32_t uniform_offset = 0;
            if(total_uniform_size > 0) {
                Utils::createBuffer(context.device, descriptor.uniform_buffer,
                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                    VK_SHARING_MODE_EXCLUSIVE,
                                    total_uniform_size);

                total_uniform_size = Utils::allocateDeviceMemory(context.pdevice, context.device,
                                                                 descriptor.uniform_buffer, descriptor.uniform_memory,
                                                                 total_uniform_size,
                                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                vkBindBufferMemory(context.device,
                                   descriptor.uniform_buffer,
                                   descriptor.uniform_memory,
                                   uniform_offset);
            }else{
                descriptor.uniform_buffer = VK_NULL_HANDLE;
                descriptor.uniform_memory = VK_NULL_HANDLE;
            }


            for (const auto& [slot, uniform] : descriptor.uniforms) {
                if(uniform.type == TYPE_BUFFER){
                    Buffer buffer{};
                    buffer.size = uniform.byte_size;
                    buffer.offset = uniform_offset;
                    uniform_offset += uniform.byte_size;
                    descriptor.buffersForSlot[slot] = buffer;

                    VkDescriptorBufferInfo info{};
                    info.buffer = descriptor.uniform_buffer;
                    info.offset = descriptor.buffersForSlot[slot].offset;
                    info.range = descriptor.buffersForSlot[slot].size;

                    VkWriteDescriptorSet dscWrite{};
                    dscWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    dscWrite.dstSet = descriptor.set;
                    dscWrite.dstBinding = slot;
                    dscWrite.dstArrayElement = 0;
                    dscWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    dscWrite.descriptorCount = 1;
                    dscWrite.pBufferInfo = &info;

                    vkUpdateDescriptorSets(context.device, 1, &dscWrite, 0, nullptr);
                }
                if(uniform.type == TYPE_IMAGE){

                    descriptor.imagesForSlot[slot] = createImage(context, {uniform.size[0], uniform.size[1]});

                    VkDescriptorImageInfo image_info;
                    image_info.imageView = descriptor.imagesForSlot[slot].imageview;
                    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    image_info.sampler = descriptor.imagesForSlot[slot].sampler;

                    VkWriteDescriptorSet dscWrite{};
                    dscWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    dscWrite.dstSet = descriptor.set;
                    dscWrite.dstBinding = slot;
                    dscWrite.dstArrayElement = 0;
                    dscWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    dscWrite.descriptorCount = 1;
                    dscWrite.pImageInfo = &image_info;

                    vkUpdateDescriptorSets(context.device, 1, &dscWrite, 0, nullptr);
                }

            }
        }
    }

}

Pipeline createPipeline(const DeviceContext& context,
        const Material& material,
        const std::vector<VkDescriptorSetLayout>& layouts,
        const VkRenderPass& render_pass,
        const VkExtent2D& extent){

    Pipeline pipeline{};
    pipeline.extent = extent;

    Utils::createShaderModule(context.device, pipeline.vertex_module, material.getVertexShader());
    Utils::createShaderModule(context.device, pipeline.fragment_module, material.getFragmentShader());

    Utils::createPipeline(context.device,
                          pipeline.pipeline,
                          pipeline.pipeline_layout,
                          layouts,
                          render_pass,
                          pipeline.vertex_module,
                          pipeline.fragment_module,
                          extent);

    return pipeline;
}

std::vector<Pipeline> createPipelines(const DeviceContext& context,
                                      const std::vector<Material>& materials,
                                      const std::vector<VkDescriptorSetLayout>& layouts,
                                      const VkRenderPass& render_pass,
                                      const VkExtent2D& extent){
    std::vector<Pipeline> pipelines(materials.size());

    for(int i = 0; i < pipelines.size(); ++i){
        pipelines[i] = createPipeline(context, materials[i], layouts, render_pass, extent);
    }

    return pipelines;
}

//Turn descriptor into a map of [slot, descriptor]
void updateUniform(const DeviceContext context,
        std::vector<DescriptorSet>& descriptors,
        const uint32_t descriptorSlot,
        const uint32_t uniformSlot){

    for (const auto& descriptor : descriptors) {
        if(descriptor.slot == descriptorSlot){
            const auto& uniform = descriptor.uniforms.at(uniformSlot);
            if(uniform.type == TYPE_BUFFER){
                const auto& buffer = descriptor.buffersForSlot.at(uniformSlot);
                Utils::copyToMemory(context.device, descriptor.uniform_memory,
                        descriptor.uniforms.at(uniformSlot).data.get(), buffer.size, buffer.offset);
            }
            if(uniform.type == TYPE_IMAGE){
                const auto& image = descriptor.imagesForSlot.at(uniformSlot);
                uploadImageData(context, {image}, {descriptor.uniforms.at(uniformSlot).data.get()}, {uniform.size}, {uniform.byte_size});
            }
        }
    }

}

void updateAllUniforms(const DeviceContext context,
        std::vector<DescriptorSet>& descriptors){
    for (const auto& descriptor : descriptors) {
        for(const auto& [slot, uniform] : descriptor.uniforms){
            if(uniform.type == TYPE_BUFFER){
                const auto& buffer = descriptor.buffersForSlot.at(slot);
                Utils::copyToMemory(context.device, descriptor.uniform_memory,
                                    descriptor.uniforms.at(slot).data.get(), buffer.size, buffer.offset);
            }
            if(uniform.type == TYPE_IMAGE){
                const auto& image = descriptor.imagesForSlot.at(slot);
                uploadImageData(context, {image}, {descriptor.uniforms.at(slot).data.get()}, {uniform.size}, {uniform.byte_size});
            }
        }
    }
}

void destroy(const DeviceContext& context, const GeometryBuffer& object){
    vkDestroyBuffer(context.device, object.buffer, nullptr);
    vkFreeMemory(context.device, object.memory, nullptr);
}
void destroy(const DeviceContext& context, const Image& image){
    vkDestroyImage(context.device, image.image, nullptr);
    vkDestroyImageView(context.device, image.imageview, nullptr);
    vkDestroySampler(context.device, image.sampler, nullptr);

    vkFreeMemory(context.device, image.imagememory, nullptr);
}
void destroy(const DeviceContext& context, const DescriptorPool& pool){
    vkDestroyDescriptorPool(context.device, pool.pool, nullptr);
}
void destroy(const DeviceContext& context, const DescriptorLayout& layout){
    vkDestroyDescriptorSetLayout(context.device, layout.layout, nullptr);
}
void destroy(const DeviceContext& context, const DescriptorSet& set){
    if(set.uniform_buffer != VK_NULL_HANDLE){
        vkDestroyBuffer(context.device, set.uniform_buffer, nullptr);
        vkFreeMemory(context.device, set.uniform_memory, nullptr);
    }

    for (const auto& [slot, image] : set.imagesForSlot) {
        destroy(context, image);
    }
}
void destroy(const DeviceContext& context, const Pipeline& pipeline){
    vkDestroyShaderModule(context.device, pipeline.vertex_module, nullptr);
    vkDestroyShaderModule(context.device, pipeline.fragment_module, nullptr);
    vkDestroyPipelineLayout(context.device, pipeline.pipeline_layout, nullptr);
    vkDestroyPipeline(context.device, pipeline.pipeline, nullptr);
}
void destroy(const DeviceContext& context, const RenderObject& object) {
    destroy(context, object.geometry);
    std::for_each(object.layouts.begin(), object.layouts.end(), [&](const auto &layout) {
        destroy(context, layout);
    });
    std::for_each(object.descriptors.begin(), object.descriptors.end(), [&](const auto &descriptor) {
        destroy(context, descriptor);
    });
    destroy(context, object.material);
}