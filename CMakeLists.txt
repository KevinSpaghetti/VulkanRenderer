cmake_minimum_required(VERSION 3.18)
project(Renderer)

set(CMAKE_CXX_STANDARD 20)
find_package(Vulkan REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W")

include_directories(libs/imgui libs/imgui/backends)

add_executable(Renderer main.cpp
        SceneGraph/BaseNode.h
        Vulkan/Renderer.h
        SceneGraph/Visitor.h
        SceneGraph/SceneGraphVisitor.h
        SceneGraph/Material.h SceneGraph/Geometry.h
        SceneGraph/Texture.h Window.h Vulkan/Logger.h
        Vulkan/Utils.h Vulkan/VulkanStructs.h
        Vulkan/Resources.h
        libs/imgui/imgui.cpp
        libs/imgui/imgui_draw.cpp
        libs/imgui/imgui_widgets.cpp
        libs/imgui/imgui_demo.cpp
        libs/imgui/imgui_tables.cpp
        libs/imgui/backends/imgui_impl_glfw.cpp
        libs/imgui/backends/imgui_impl_vulkan.cpp libs/stbi_image.h)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(libs/glfw)

target_include_directories(Renderer PRIVATE "ENV(VULKAN_SDK)/Include")

target_link_libraries(Renderer glfw)
target_link_libraries(Renderer Vulkan::Vulkan)
