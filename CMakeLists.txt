cmake_minimum_required(VERSION 3.16)
project(Renderer)

set(CMAKE_CXX_STANDARD 20)
find_package(Vulkan REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

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
add_subdirectory(libs/glm)

target_include_directories(Renderer PRIVATE "ENV(VULKAN_SDK)/Include")

target_link_libraries(Renderer glfw)
target_link_libraries(Renderer glm)
target_link_libraries(Renderer Vulkan::Vulkan)

# Compile shaders to build directory with glslc
add_custom_command(TARGET Renderer
        POST_BUILD
        COMMENT "Compiling shaders into sprv"
        COMMAND glslc ${CMAKE_SOURCE_DIR}/resources/shaders/compute.comp -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/compute.sprv
        COMMAND glslc ${CMAKE_SOURCE_DIR}/resources/shaders/cube.frag -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cubef.sprv
        COMMAND glslc ${CMAKE_SOURCE_DIR}/resources/shaders/cube.vert -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cubev.sprv
        COMMAND glslc ${CMAKE_SOURCE_DIR}/resources/shaders/helmet.frag -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/helmetf.sprv
        COMMAND glslc ${CMAKE_SOURCE_DIR}/resources/shaders/helmet.vert -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/helmetv.sprv
        COMMAND glslc ${CMAKE_SOURCE_DIR}/resources/shaders/image.frag -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/image.sprv
        COMMAND glslc ${CMAKE_SOURCE_DIR}/resources/shaders/lightmap.vert -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/lightmap.sprv
        COMMAND glslc ${CMAKE_SOURCE_DIR}/resources/shaders/plane.frag -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/planef.sprv
        COMMAND glslc ${CMAKE_SOURCE_DIR}/resources/shaders/vert.vert -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/planev.sprv
        )