//
// Created by Kevin on 23/12/2020.
//

#pragma once

#include <iostream>
#include <vector>
#include <array>
#include <numeric>
#include "../SceneGraph/BaseNode.h"
#include "Logger.h"
#include "Utils.h"
#include "../SceneGraph/SceneGraphVisitor.h"
#include "VulkanStructs.h"
#include "../libs/imgui/imgui.h"
#include "../libs/imgui/backends/imgui_impl_glfw.h"
#include "../libs/imgui/backends/imgui_impl_vulkan.h"

class Renderer {
private:

    struct InstanceData {
        VkInstance instance;

        const std::vector<const char *> device_layers = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        const std::vector<const char *> validation_layers = {"VK_LAYER_KHRONOS_validation"};
    } m_instance_data;

    VkPhysicalDevice m_pdevice;
    VkDevice m_device;
    VkSurfaceKHR m_surface;

    VkRenderPass m_render_pass;

    struct SwapchainInfo {
        VkSwapchainKHR swapchain;

        VkExtent2D extent{0, 0};

        uint32_t nImages{0};
        std::vector<VkImage> images{};
        std::vector<VkImageView> views{};
        std::vector<VkFramebuffer> framebuffers{};

        std::vector<VkFramebuffer> target_buffers{};
    } m_swapchain_data;

    struct CommandData {
        VkCommandPool pool;
        std::vector<VkCommandBuffer> buffers;
    } m_commands;

    struct QueueInfo {
        uint32_t graphicsFamilyindex;
        VkQueue graphics;
        uint32_t graphicsQueueIndex;

        uint32_t transferFamilyindex;
        VkQueue transfer;
        uint32_t transferQueueIndex;

        uint32_t computeFamilyindex;
        VkQueue compute;
        uint32_t computeQueueIndex;
    } m_queue_info;

    struct image {
        VkImage image;
        VkDeviceMemory memory;
        VkImageView view;
    } depthImage;

    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;

    std::map<std::string, RenderObject> loadedObjects;

    std::vector<GeometryBuffer> loadedMeshes;
    std::vector<Material> loadedPipelines;
    std::vector<DescriptorLayout> loadedLayouts;

    const CameraNode* activeCamera = nullptr;

public:
    Renderer(const Window &window) {
        createVulkanResources(window);
        initImguiInstance(window);
    }

    struct FrameLocalData {
        VkCommandBuffer command;
        uint32_t image_index;

        VkImageView image_view;
        VkFramebuffer framebuffer;
        VkFramebuffer fill_target;
    };

    DescriptorPool pool;

    void setCamera(const CameraNode* camera){
        activeCamera = camera;
    }

    void load(const std::vector<ObjectNode*>& toLoad) {

        std::vector<ObjectNode*> notAlreadyLoadedObjects;
        std::copy_if(toLoad.begin(), toLoad.end(),
                std::back_inserter(notAlreadyLoadedObjects),
                [&](const auto& object){
                    return !loadedObjects.contains(object->name());
                });
        if(notAlreadyLoadedObjects.empty()) {
            return ;
        }

        const DeviceContext context{m_pdevice, m_device, m_queue_info.graphics, m_queue_info.graphicsFamilyindex};

        std::vector<Geometry> geometries;
        std::vector<Material> materials;
        geometries.reserve(notAlreadyLoadedObjects.size());
        materials.reserve(notAlreadyLoadedObjects.size());

        std::for_each(notAlreadyLoadedObjects.begin(), notAlreadyLoadedObjects.end(), [&geometries](const auto& o){ geometries.push_back(o->getGeometry());});
        std::for_each(notAlreadyLoadedObjects.begin(), notAlreadyLoadedObjects.end(), [&materials](const auto& o){ materials.push_back(o->getMaterial());});

        std::vector<GeometryBuffer> geom_buffers = createBuffers(context, geometries);

        std::vector<std::vector<UniformSet>> objectSets(notAlreadyLoadedObjects.size());
        std::transform(notAlreadyLoadedObjects.begin(), notAlreadyLoadedObjects.end(),
                objectSets.begin(),
                [](const auto& object) {return object->getUniformSets();});

        pool = createDescriptorPool(context, objectSets);

        std::vector<std::vector<DescriptorLayout>> objectLayouts = createDescriptorLayouts(context, objectSets);
        std::vector<std::vector<DescriptorSet>> objectDescriptors = createDescriptorSets(context, objectSets, objectLayouts, pool);

        initDescriptorSets(context, objectDescriptors);

        std::vector<Pipeline> pipelines(materials.size());

        for (int i = 0; i < materials.size(); ++i){
            std::vector<VkDescriptorSetLayout> layouts(objectLayouts[i].size());
            std::transform(objectLayouts[i].begin(), objectLayouts[i].end(), layouts.begin(),
                    [](const auto& object){ return object.layout; });
            pipelines[i] = createPipeline(context,
                    materials[i],
                    layouts,
                    m_render_pass,
                    m_swapchain_data.extent);
        }

        for(int i = 0; i < notAlreadyLoadedObjects.size(); ++i) {
            Logger::log("loaded: " + notAlreadyLoadedObjects[i]->name() + "\n");
            loadedObjects.insert({
                    notAlreadyLoadedObjects[i]->name(),
                    RenderObject{
                        notAlreadyLoadedObjects[i],
                        geom_buffers[i],
                        objectLayouts[i],
                        objectDescriptors[i],
                        pipelines[i],
                    }});
        }

        for(auto& descriptors : objectDescriptors){
            updateAllUniforms(context, descriptors);
        }
    }

    void updateUniforms(){
        const DeviceContext context{m_pdevice, m_device, m_queue_info.graphics, m_queue_info.graphicsFamilyindex};
        for (auto& [key, object] : loadedObjects) {
            if(object.node->toUpdate()){
                //Update object uniform
                matrices m{};
                m.model = object.node->modelMatrix();
                m.view = activeCamera->getViewMatrix();
                m.projection = activeCamera->getProjectionMatrix();

                glm::vec3 cameraPosition = glm::vec3(glm::vec4(0.0, 0.0, 0.0, 1.0) * activeCamera->modelMatrix());

                memcpy(object.descriptors[0].uniforms[0].data.get(), &m.model, object.descriptors[0].uniforms[0].byte_size);
                memcpy(object.descriptors[0].uniforms[1].data.get(), &m.view, object.descriptors[0].uniforms[1].byte_size);
                memcpy(object.descriptors[0].uniforms[2].data.get(), &m.projection, object.descriptors[0].uniforms[2].byte_size);
                memcpy(object.descriptors[0].uniforms[3].data.get(), &cameraPosition, object.descriptors[0].uniforms[3].byte_size);

                updateUniform(context, object.descriptors, 0, 0);
                updateUniform(context, object.descriptors, 0, 1);
                updateUniform(context, object.descriptors, 0, 2);
                updateUniform(context, object.descriptors, 0, 3);

                object.node->updated();
            }
        }
    }

    void unload(const std::vector<std::string>& namesOfObjectsToUnload) {
        const DeviceContext context{m_pdevice, m_device, m_queue_info.graphics, m_queue_info.graphicsFamilyindex};

        for(const auto& objectName: namesOfObjectsToUnload){
            destroy(context, loadedObjects[objectName]);
            loadedObjects.erase(objectName);
            Logger::log("unloaded: " + objectName + " \n");
        }
    }

    void render() {
        FrameLocalData frameData{};
        vkAcquireNextImageKHR(m_device,
                              m_swapchain_data.swapchain,
                              UINT64_MAX,
                              imageAvailableSemaphore,
                              VK_NULL_HANDLE,
                              &frameData.image_index);
        frameData.command = m_commands.buffers[frameData.image_index];
        frameData.image_view = m_swapchain_data.views[frameData.image_index];
        frameData.framebuffer = m_swapchain_data.framebuffers[frameData.image_index];
        frameData.fill_target = fill_texture_fbs[frameData.image_index];

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr; // Optional
        if (vkBeginCommandBuffer(frameData.command, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        recordCommandsInto(frameData.command,
                           frameData,
                           m_swapchain_data,
                           m_render_pass);

        if (vkEndCommandBuffer(frameData.command) != VK_SUCCESS) {
            throw std::runtime_error("failed to end recording command buffer!");
        }

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &frameData.command;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkQueueSubmit(m_queue_info.graphics, 1, &submitInfo, VK_NULL_HANDLE);

        VkSwapchainKHR swapchains[] = {m_swapchain_data.swapchain};
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &frameData.image_index;
        presentInfo.pResults = nullptr;
        vkQueuePresentKHR(m_queue_info.graphics, &presentInfo);

        vkQueueWaitIdle(m_queue_info.graphics);
    }

    ~Renderer() {
        vkDeviceWaitIdle(m_device);

        const DeviceContext context{m_pdevice, m_device, m_queue_info.graphics, m_queue_info.graphicsFamilyindex};
        for(const auto& [key, object]: loadedObjects){
            destroy(context, object);
        }
        destroy(context, pool);

        vkDestroySemaphore(m_device, imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(m_device, renderFinishedSemaphore, nullptr);

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        destroy(context, render_target);

        vkDestroyDescriptorPool(m_device, guiPool, nullptr);

        vkDestroyRenderPass(m_device, fill_texture, nullptr);
        std::for_each(fill_texture_fbs.begin(), fill_texture_fbs.end(), [&](auto& fb){
            vkDestroyFramebuffer(m_device, fb, nullptr);
        });

        vkDestroyCommandPool(m_device, m_commands.pool, nullptr);
        for (auto framebuffer : m_swapchain_data.framebuffers) {
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }
        vkDestroyRenderPass(m_device, m_render_pass, nullptr);
        for (auto iv : m_swapchain_data.views) {
            vkDestroyImageView(m_device, iv, nullptr);
        }

        vkDestroyImageView(m_device, depthImage.view, nullptr);
        vkDestroyImage(m_device, depthImage.image, nullptr);
        vkFreeMemory(m_device, depthImage.memory, nullptr);

        vkDestroySwapchainKHR(m_device, m_swapchain_data.swapchain, nullptr);
        vkDestroySurfaceKHR(m_instance_data.instance, m_surface, nullptr);
        vkDestroyDevice(m_device, nullptr);
        vkDestroyInstance(m_instance_data.instance, nullptr);
    }

private:

    VkDescriptorPool guiPool;

    void initImguiInstance(const Window& window){
        assert(m_instance_data.instance != VK_NULL_HANDLE);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();

        ImGui::StyleColorsDark();

        const std::array<VkDescriptorPoolSize, 11> imgui_pool_sizes{{
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
        }};

        VkDescriptorPoolCreateInfo pinfo{};
        pinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pinfo.poolSizeCount = imgui_pool_sizes.size();
        pinfo.pPoolSizes = imgui_pool_sizes.data();
        pinfo.maxSets = 64;

        vkCreateDescriptorPool(m_device, &pinfo, nullptr, &guiPool);

        ImGui_ImplGlfw_InitForVulkan(window.getWindowHandle(), true);
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = m_instance_data.instance;
        init_info.PhysicalDevice = m_pdevice;
        init_info.Device = m_device;
        init_info.QueueFamily = m_queue_info.graphicsFamilyindex;
        init_info.Queue = m_queue_info.graphics;
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = guiPool;
        init_info.Allocator = VK_NULL_HANDLE;
        init_info.MinImageCount = m_swapchain_data.nImages;
        init_info.ImageCount = m_swapchain_data.nImages;
        init_info.CheckVkResultFn = VK_NULL_HANDLE;
        ImGui_ImplVulkan_Init(&init_info, m_render_pass);

        VkCommandPool createResourcesPool{};
        VkCommandBuffer command;

        VkCommandPoolCreateInfo poolinfo{};
        poolinfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolinfo.queueFamilyIndex = m_queue_info.graphicsFamilyindex;
        poolinfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if (vkCreateCommandPool(m_device, &poolinfo, nullptr, &createResourcesPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool");
        }

        VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.commandPool = createResourcesPool;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocateInfo.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(m_device, &commandBufferAllocateInfo, &command) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers");
        }

        VkCommandBufferBeginInfo info{};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(command, &info);

        ImGui_ImplVulkan_CreateFontsTexture(command);

        vkEndCommandBuffer(command);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &command;
        vkQueueSubmit(m_queue_info.graphics, 1, &submitInfo, VK_NULL_HANDLE);

        vkQueueWaitIdle(m_queue_info.graphics);

        vkDestroyCommandPool(m_device, createResourcesPool, nullptr);
    }

    Image render_target{};
    VkRenderPass fill_texture{};
    std::vector<VkFramebuffer> fill_texture_fbs;

    void createVulkanResources(const Window &window) {
        VkApplicationInfo appinfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        appinfo.pApplicationName = "Raytracer";
        appinfo.pEngineName = "Vulkan image presenter";
        appinfo.apiVersion = VK_MAKE_VERSION(1, 0, 0);

        uint32_t windowSystemExtensionCount = 0;
        const char **windowSystemExtensions = glfwGetRequiredInstanceExtensions(&windowSystemExtensionCount);

        VkInstanceCreateInfo instanceinfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        instanceinfo.pApplicationInfo = &appinfo;
        instanceinfo.enabledExtensionCount = windowSystemExtensionCount;
        instanceinfo.ppEnabledExtensionNames = windowSystemExtensions;
        instanceinfo.enabledLayerCount = static_cast<uint32_t>(m_instance_data.validation_layers.size());
        instanceinfo.ppEnabledLayerNames = m_instance_data.validation_layers.data();

        Logger::log("Enabled " + std::to_string(windowSystemExtensionCount) + " extensions for window system\n");
        for (int i = 0; i < windowSystemExtensionCount; ++i) {
            Logger::log("\t" + std::string(windowSystemExtensions[i]) + "\n");
        }
        Logger::log("Enabled " + std::to_string(m_instance_data.validation_layers.size()) + " layers\n");
        for (auto layer: m_instance_data.validation_layers) {
            Logger::log("\t" + std::string(layer) + "\n");
        };

        if (vkCreateInstance(&instanceinfo, nullptr, &m_instance_data.instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create instance");
        }

        //Pick the physical m_device
        uint32_t pdevicecount = 0;
        vkEnumeratePhysicalDevices(m_instance_data.instance, &pdevicecount, nullptr);
        if (pdevicecount == 0) throw std::runtime_error("No physical device found");
        std::vector<VkPhysicalDevice> pdevices(pdevicecount);
        vkEnumeratePhysicalDevices(m_instance_data.instance, &pdevicecount, pdevices.data());

        Logger::log("" + std::to_string(pdevicecount) + " physical devices found\n");
        for (auto pdevice: pdevices) {
            VkPhysicalDeviceProperties device_props;
            vkGetPhysicalDeviceProperties(pdevice, &device_props);
            Logger::log("\t" + std::string(device_props.deviceName) + "\n");
        }

        m_pdevice = pdevices.front();

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_pdevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueprops(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_pdevice, &queueFamilyCount, queueprops.data());
        Logger::log("" + std::to_string(queueFamilyCount) + " queue types in the physical device\n");
        for (auto queue_prop: queueprops) {
            Logger::log("\t" +
                        Utils::getReadableQueueFlags(queue_prop.queueFlags) +
                        ": " +
                        std::to_string(queue_prop.queueCount) + "\n");
        }

        m_queue_info.graphicsFamilyindex = Utils::indexOfQueueFamilyWithFlags(queueprops, VK_QUEUE_GRAPHICS_BIT);
        m_queue_info.transferFamilyindex = 1;
        m_queue_info.computeFamilyindex = 2;

        m_queue_info.graphicsQueueIndex = 0;
        m_queue_info.transferQueueIndex = 0;
        m_queue_info.computeQueueIndex = 0;

        //Logical m_device
        float qpriorities[] = {1.0f, 0.8f};
        std::array<VkDeviceQueueCreateInfo, 3> queueCreateInfos{};
        queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[0].queueFamilyIndex = m_queue_info.graphicsFamilyindex;
        queueCreateInfos[0].queueCount = 1;
        queueCreateInfos[0].pQueuePriorities = qpriorities;
        queueCreateInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[1].queueFamilyIndex = m_queue_info.transferFamilyindex;
        queueCreateInfos[1].queueCount = 1;
        queueCreateInfos[1].pQueuePriorities = qpriorities;
        queueCreateInfos[2].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[2].queueFamilyIndex = m_queue_info.computeFamilyindex;
        queueCreateInfos[2].queueCount = 1;
        queueCreateInfos[2].pQueuePriorities = qpriorities;

        VkPhysicalDeviceFeatures device_features{};
        device_features.samplerAnisotropy = VK_TRUE;

        VkDeviceCreateInfo deviceinfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        deviceinfo.queueCreateInfoCount = queueCreateInfos.size();
        deviceinfo.pQueueCreateInfos = queueCreateInfos.data();
        deviceinfo.pEnabledFeatures = &device_features;
        deviceinfo.enabledExtensionCount = static_cast<uint32_t>(m_instance_data.device_layers.size());
        deviceinfo.ppEnabledExtensionNames = m_instance_data.device_layers.data();
        deviceinfo.enabledLayerCount = static_cast<uint32_t>(m_instance_data.validation_layers.size());
        deviceinfo.ppEnabledLayerNames = m_instance_data.validation_layers.data();

        if (vkCreateDevice(m_pdevice, &deviceinfo, nullptr, &m_device)) {
            throw std::runtime_error("Logical device creation failed");
        }

        vkGetDeviceQueue(m_device, m_queue_info.graphicsFamilyindex, m_queue_info.graphicsQueueIndex, &m_queue_info.graphics);
        vkGetDeviceQueue(m_device, m_queue_info.transferFamilyindex, m_queue_info.transferQueueIndex, &m_queue_info.transfer);
        vkGetDeviceQueue(m_device, m_queue_info.computeFamilyindex, m_queue_info.computeQueueIndex, &m_queue_info.compute);

        if (glfwCreateWindowSurface(m_instance_data.instance, window.getWindowHandle(), nullptr, &m_surface) !=
            VK_SUCCESS) {
            throw std::runtime_error("Surface creation failed");
        }

        auto window_size = window.getWindowSize();
        m_swapchain_data.extent = {
                static_cast<uint32_t>(window_size.x),
                static_cast<uint32_t>(window_size.y)
        };

        SurfaceParams surface_params{};
        createSwapchain(m_pdevice, m_device, m_surface,
                        m_swapchain_data.swapchain,
                        m_queue_info.graphicsFamilyindex,
                        m_swapchain_data.extent,
                        surface_params);

        vkGetSwapchainImagesKHR(m_device,
                                m_swapchain_data.swapchain,
                                &m_swapchain_data.nImages,
                                nullptr);

        m_swapchain_data.images.resize(m_swapchain_data.nImages);
        m_swapchain_data.views.resize(m_swapchain_data.nImages);
        m_swapchain_data.framebuffers.resize(m_swapchain_data.nImages);

        vkGetSwapchainImagesKHR(m_device,
                                m_swapchain_data.swapchain,
                                &m_swapchain_data.nImages,
                                m_swapchain_data.images.data());

        for (int i = 0; i < m_swapchain_data.nImages; ++i) {
            Utils::createImageView(m_device,
                                   m_swapchain_data.views[i],
                                   m_swapchain_data.images[i],
                                   surface_params.format.format,
                                   VK_IMAGE_ASPECT_COLOR_BIT);
        }

        VkFormat depthFormat = Utils::findDepthFormat(m_pdevice);
        Utils::createImage(m_pdevice, m_device,
                           depthImage.image,
                           depthImage.memory,
                           m_swapchain_data.extent,
                           depthFormat,
                           VK_IMAGE_TILING_OPTIMAL,
                           VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Utils::createImageView(m_device,
                               depthImage.view,
                               depthImage.image,
                               depthFormat,
                               VK_IMAGE_ASPECT_DEPTH_BIT);


        const DeviceContext context{m_pdevice, m_device, m_queue_info.graphics, m_queue_info.graphicsFamilyindex};
        render_target = createImage(context, {
            m_swapchain_data.extent.width,
            m_swapchain_data.extent.height
        });

        fill_texture_fbs.resize(m_swapchain_data.nImages);

        createRenderPass(fill_texture, m_pdevice, m_device,
                         m_swapchain_data.extent,
                         depthFormat,
                         surface_params.format.format,
                         VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        createRenderPass(m_render_pass, m_pdevice, m_device,
                         m_swapchain_data.extent,
                         depthFormat,
                         surface_params.format.format,
                         VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        for (size_t i = 0; i < m_swapchain_data.nImages; ++i) {
            std::vector<VkImageView> attachments2 = {
                    render_target.imageview,
                    depthImage.view
            };
            createFramebuffer(m_device,
                              fill_texture,
                              fill_texture_fbs[i],
                              m_swapchain_data.extent,
                              attachments2);

            std::vector<VkImageView> attachments = {
                    m_swapchain_data.views[i],
                    depthImage.view
            };
            createFramebuffer(m_device,
                              m_render_pass,
                              m_swapchain_data.framebuffers[i],
                              m_swapchain_data.extent,
                              attachments);
        }

        createCommandBuffers(m_device,
                             m_commands.pool,
                             m_commands.buffers,
                             m_swapchain_data.nImages,
                             m_queue_info.graphicsFamilyindex,
                             VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

        VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        vkCreateSemaphore(m_device, &semInfo, nullptr, &imageAvailableSemaphore);
        vkCreateSemaphore(m_device, &semInfo, nullptr, &renderFinishedSemaphore);

    }

    void createSwapchain(const VkPhysicalDevice &pdevice,
                         const VkDevice &device,
                         const VkSurfaceKHR &surface,
                         VkSwapchainKHR &swapchain,
                         int presentQueueIndex,
                         const VkExtent2D &extent,
                         SurfaceParams &outParams) {

        VkBool32 supported;
        vkGetPhysicalDeviceSurfaceSupportKHR(pdevice, presentQueueIndex, surface, &supported);
        std::string bv = (supported ? "true" : "false");
        Logger::log("Device and surface support presenting? : " + bv + "\n");

        outParams = Utils::chooseSurfaceParams(pdevice,
                                               surface,
                                               VkSurfaceFormatKHR{VK_FORMAT_R8G8B8A8_SRGB,
                                                                  VK_COLORSPACE_SRGB_NONLINEAR_KHR},
                                               VK_PRESENT_MODE_FIFO_KHR);

        VkSwapchainCreateInfoKHR swapchain_createinfo{};
        swapchain_createinfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_createinfo.surface = surface;
        swapchain_createinfo.minImageCount = outParams.capabilities.minImageCount + 1;
        swapchain_createinfo.imageFormat = outParams.format.format;
        swapchain_createinfo.imageColorSpace = outParams.format.colorSpace;
        swapchain_createinfo.imageExtent = extent;
        swapchain_createinfo.imageArrayLayers = 1;
        swapchain_createinfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchain_createinfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_createinfo.queueFamilyIndexCount = 0;
        swapchain_createinfo.pQueueFamilyIndices = nullptr;
        swapchain_createinfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain_createinfo.presentMode = outParams.present_mode;
        swapchain_createinfo.clipped = VK_TRUE;
        swapchain_createinfo.preTransform = outParams.capabilities.currentTransform;
        swapchain_createinfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(device, &swapchain_createinfo, nullptr, &swapchain) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create m_swapchain");
        }
    }

    void createRenderPass(VkRenderPass &render_pass,
                          const VkPhysicalDevice &pdevice,
                          const VkDevice &device,
                          const VkExtent2D &extent,
                          const VkFormat depth_format,
                          const VkFormat color_format,
                          const VkImageLayout color_attachment_initial_layout,
                          const VkImageLayout color_attachment_final_layout) {

        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = color_format;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = color_attachment_initial_layout;
        colorAttachment.finalLayout = color_attachment_final_layout;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = depth_format;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask =
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask =
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 2> attachments{colorAttachment, depthAttachment};

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 2;
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &render_pass) != VK_SUCCESS) {
            throw std::runtime_error("Failed render pass creation");
        }
    }

    void createFramebuffer(const VkDevice device,
                           const VkRenderPass render_pass,
                           VkFramebuffer &framebuffer,
                           const VkExtent2D extent,
                           const std::vector<VkImageView> attachments) {
        VkFramebufferCreateInfo fbinfo{};
        fbinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbinfo.renderPass = render_pass;
        fbinfo.attachmentCount = attachments.size();
        fbinfo.pAttachments = attachments.data();
        fbinfo.width = extent.width;
        fbinfo.height = extent.height;
        fbinfo.layers = 1;

        if (vkCreateFramebuffer(device, &fbinfo, nullptr, &framebuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }

    void createCommandBuffers(const VkDevice device,
                              VkCommandPool &pool,
                              std::vector<VkCommandBuffer> &commandBuffers,
                              const uint32_t nOfCommandBuffers,
                              const uint32_t queueIndex,
                              const VkCommandPoolCreateFlags poolCreateFlags) {

        VkCommandPoolCreateInfo poolinfo{};
        poolinfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolinfo.queueFamilyIndex = queueIndex;
        poolinfo.flags = poolCreateFlags;

        if (vkCreateCommandPool(device, &poolinfo, nullptr, &pool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool");
        }

        commandBuffers.resize(nOfCommandBuffers);
        VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.commandPool = pool;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocateInfo.commandBufferCount = static_cast<uint32_t>(nOfCommandBuffers);

        if (vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers");
        }

    }

    void recordCommandsInto(VkCommandBuffer &command,
                            const FrameLocalData &frame_data,
                            const SwapchainInfo &swapchain_info,
                            const VkRenderPass &render_pass) {

        constexpr std::array<VkClearValue, 2> clearVals1{
                {{0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}}
        };
        constexpr std::array<VkClearValue, 2> clearVals{
                {{0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}
        };

        VkRenderPassBeginInfo render_to_target_info{};
        render_to_target_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_to_target_info.renderPass = fill_texture;
        render_to_target_info.framebuffer = frame_data.fill_target;
        render_to_target_info.renderArea.offset = {0, 0};
        render_to_target_info.renderArea.extent = swapchain_info.extent;
        render_to_target_info.clearValueCount = clearVals1.size();
        render_to_target_info.pClearValues = clearVals1.data();

        vkCmdBeginRenderPass(command, &render_to_target_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdEndRenderPass(command);

        VkRenderPassBeginInfo renderpassbegininfo{};
        renderpassbegininfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderpassbegininfo.renderPass = render_pass;
        renderpassbegininfo.framebuffer = frame_data.framebuffer;
        renderpassbegininfo.renderArea.offset = {0, 0};
        renderpassbegininfo.renderArea.extent = swapchain_info.extent;
        renderpassbegininfo.clearValueCount = clearVals.size();
        renderpassbegininfo.pClearValues = clearVals.data();

        vkCmdBeginRenderPass(command, &renderpassbegininfo, VK_SUBPASS_CONTENTS_INLINE);
        for (const auto& [name, object]  : loadedObjects) {

            vkCmdBindPipeline(command,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              object.material.pipeline);

            std::vector<VkDescriptorSet> sets(object.descriptors.size());
            std::transform(object.descriptors.begin(), object.descriptors.end(), sets.begin(),
                    [](const auto& o) { return o.set;} );

            vkCmdBindDescriptorSets(command,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    object.material.pipeline_layout,
                                    0, sets.size(),
                                    sets.data(),
                                    0, 0);

            VkDeviceSize offsets[1] = {object.geometry.vertices_offset};
            vkCmdBindVertexBuffers(command,
                                   0, 1,
                                   &object.geometry.buffer,
                                   offsets);
            vkCmdBindIndexBuffer(command,
                                 object.geometry.buffer,
                                 object.geometry.indices_offset,
                                 VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(command,
                             object.geometry.n_of_indices,
                             1,
                             0,
                             0,
                             0);
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::ShowDemoWindow();
        ImGui::Render();

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command);

        vkCmdEndRenderPass(command);
    }
};