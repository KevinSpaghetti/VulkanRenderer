#include <iostream>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <chrono>
#include <algorithm>
#include "Window.h"
#include "Vulkan/Renderer.h"
#define STB_IMAGE_IMPLEMENTATION
#include "libs/stbi_image.h"

Texture2D load(const std::string& filepath){
    glm::ivec2 size;
    int channels;

    unsigned char* pixels = stbi_load(filepath.c_str(), &size.x, &size.y, &channels, STBI_rgb_alpha);
    if(pixels){
        return Texture2D(size, STBI_rgb_alpha, pixels);
    }

    return Texture2D();
}

std::vector<char> readFile(const std::string& filepath){
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("File not found");
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> code(fileSize);
    file.seekg(0);
    file.read(code.data(), fileSize);
    file.close();

    return code;
}

std::shared_ptr<ObjectNode> loadObjFile(const std::string& name, const std::string& basedir, const std::string& filename){

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warn;
    std::string errs;

    std::string completeFilePath = basedir + filename;
    tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &errs, completeFilePath.c_str(), basedir.c_str());

    std::cout << warn << "\n";
    std::cout << errs << "\n";

    std::vector<VertexData> vdata(attrib.vertices.size());
    std::vector<uint32_t> indices(shapes[0].mesh.indices.size());

    for (int i = 0, j = 0; i < attrib.vertices.size(); i=i+3, ++j){
        vdata[j].position = {
                attrib.vertices[i],
                attrib.vertices[i+1],
                attrib.vertices[i+2]
        };
    }
    for (int i = 0; i < shapes[0].mesh.indices.size(); ++i) {
        auto index = shapes[0].mesh.indices[i];

        indices[i] = index.vertex_index;
        vdata[index.vertex_index].normal_1 = {
                static_cast<float>(attrib.normals[3 * index.normal_index]),
                static_cast<float>(attrib.normals[3 * index.normal_index+1]),
                static_cast<float>(attrib.normals[3 * index.normal_index+2])
        };
        vdata[index.vertex_index].texcoord_1 = {
                static_cast<float>(attrib.texcoords[2 * index.texcoord_index]),
                1.0f - static_cast<float>(attrib.texcoords[2 * index.texcoord_index+1])
        };
    }

    Geometry geometry(std::move(indices), std::move(vdata));

    std::vector<char> vscode = readFile("helmetv.sprv");
    std::vector<char> fscode = readFile("helmetf.sprv");

    Material material(materials[0].name, vscode, fscode);
    std::map<uint32_t, Texture2D> textures;

    textures.emplace(0, load(materials[0].diffuse_texname));
    textures.emplace(1, load(materials[0].bump_texname));
    textures.emplace(2, load(materials[0].specular_texname));
    textures.emplace(3, load(materials[0].emissive_texname));

    for(const auto& [location, txt] : textures) {
        const auto uniform = Uniform{
                .type = TYPE_IMAGE,
                .size = {static_cast<uint32_t>(txt.size().x), static_cast<uint32_t>(txt.size().y),0},
                .byte_size = txt.data_size(),
                .count = 1,
                .data = txt.data()
        };

        material.uniform(location) = uniform;
    }

    return std::make_shared<ObjectNode>(name, geometry, material);
}

std::shared_ptr<ObjectNode> loadPlane(const std::string& name, const std::string& basedir, const std::string& filename){

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warn;
    std::string errs;

    std::string completeFilePath = basedir + filename;
    tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &errs, completeFilePath.c_str(), basedir.c_str());

    std::cout << warn << "\n";
    std::cout << errs << "\n";

    std::vector<VertexData> vdata(attrib.vertices.size());
    std::vector<uint32_t> indices(shapes[0].mesh.indices.size());

    for (int i = 0, j = 0; i < attrib.vertices.size(); i=i+3, ++j){
        vdata[j].position = {
                attrib.vertices[i],
                attrib.vertices[i+1],
                attrib.vertices[i+2]
        };
    }

    for (int i = 0; i < shapes[0].mesh.indices.size(); ++i) {
        auto index = shapes[0].mesh.indices[i];

        indices[i] = index.vertex_index;
        glm::vec2 crd = {
                static_cast<float>(attrib.texcoords[2 * index.texcoord_index]),
                1.0f - static_cast<float>(attrib.texcoords[2 * index.texcoord_index+1])
        };

        vdata[index.vertex_index].normal_1 = {
                static_cast<float>(attrib.normals[3 * index.normal_index]),
                static_cast<float>(attrib.normals[3 * index.normal_index+1]),
                static_cast<float>(attrib.normals[3 * index.normal_index+2])
        };
        vdata[index.vertex_index].texcoord_1 = crd;
    }

    Geometry geometry(std::move(indices), std::move(vdata));

    std::vector<char> vscode = readFile("vert.sprv");
    std::vector<char> fscode = readFile("planef.sprv");

    Material material("mt", vscode, fscode);

    return std::make_shared<ObjectNode>(name, geometry, material);
}

void visitTree(BaseNode* root, Visitor *visitor) {
    root->accept(visitor);
    for (const auto& child : root->children()) {
        visitTree(child.get(), visitor);
    }
}

int main() {

    const int width = 1600;
    const int height = 900;
    const std::string title = "Vulkan";

    BaseNode root("root");

    auto camera = std::make_shared<CameraNode>("main camera", true, 45.0f, width, height, 0.1, 1000.0);
    root.addChild(camera);

    auto model = loadObjFile("helmet_1", "../resources/HelmetModel/", "helmet.obj");
    auto plane = loadPlane("plane", "../resources/", "plane.obj");

    plane->setTranslation({0, -1.5, 0});
    root.addChild(model);
    root.addChild(plane);

    auto light1 = std::make_shared<LightNode>(LightNode("light_1", 10.0, {1.0, 1.0, 1.0}, 20));
    auto light2 = std::make_shared<LightNode>(LightNode("light_2", 10.0, {1.0, 1.0, 1.0}, 20));
    auto light3 = std::make_shared<LightNode>(LightNode("light_3", 10.0, {1.0, 1.0, 1.0}, 20));

    light1->setTranslation({-2.0, 1.0, 5.0});
    light2->setTranslation({0.0, 1.0, 5.0});
    light3->setTranslation({2.0, 1.0, 5.0});

    light1->setRotation({1, 0, 0}, glm::radians(45.0f));
    light2->setRotation({1, 0, 0}, glm::radians(45.0f));
    light3->setRotation({1, 0, 0}, glm::radians(45.0f));

    root.addChild(light1);
    root.addChild(light2);
    root.addChild(light3);

    Window window(title, width, height);

    Renderer renderer(window);

    glm::vec3 rotation(0, 0.0, 0.0);
    glm::vec3 translation({0.0, 0.0, 5.0});
    root.toUpdate();
    glm::dvec2 start_pos{};

    CollectObjectsVisitor objectsVisitor;
    visitTree(&root, &objectsVisitor);
    CollectLightsVisitor lightsVisitor;
    visitTree(&root, &lightsVisitor);

    FindActiveCameraVisitor cameraVisitor;
    visitTree(&root, &cameraVisitor);

    std::vector<ObjectNode*> objects = objectsVisitor.collected();
    std::vector<LightNode*> lights = lightsVisitor.collected();

    const CameraNode* cameraNode = cameraVisitor.collected();

    renderer.setCamera(cameraNode);
    renderer.setLights(lights);
    renderer.load(objects);

    while(!window.windowShouldClose()) {
        window.pollEvents();

        constexpr float movement_delta = 0.10f;
        GLFWwindow *wd = window.getWindowHandle();
        if (glfwGetKey(wd, GLFW_KEY_W) == GLFW_PRESS) {
            translation.z -= movement_delta;
        }
        if (glfwGetKey(wd, GLFW_KEY_A) == GLFW_PRESS) {
            translation.x -= movement_delta;
        }
        if (glfwGetKey(wd, GLFW_KEY_S) == GLFW_PRESS) {
            translation.z += movement_delta;
        }
        if (glfwGetKey(wd, GLFW_KEY_D) == GLFW_PRESS) {
            translation.x += movement_delta;
        }
        if(glfwGetMouseButton(wd, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS){
            const glm::vec2 rotation_angle_min = {0.0, 0.0};
            const glm::vec2 rotation_angle_max = {90.0, 90.0};

            glm::dvec2 pos;
            int w_width, w_height;
            glfwGetCursorPos(wd, &pos.x, &pos.y);
            glfwGetWindowSize(wd, &w_width, &w_height);

            glm::vec2 delta = pos - start_pos;
            glm::vec2 wsize = {
                    static_cast<float>(w_width),
                    static_cast<float>(w_height)};
            glm::vec2 norm_delta = delta / wsize;

            glm::vec2 rot = glm::mix(rotation_angle_min, rotation_angle_max, norm_delta);

            rotation.y += rot.x;
            rotation.x += rot.y;

            start_pos = pos;

        }else{
            rotation.y -= (3.14 / 5.0);
            glfwGetCursorPos(wd, &start_pos.x, &start_pos.y);
        }
        if(glfwGetKey(wd, GLFW_KEY_SPACE) == GLFW_PRESS){
            rotation = glm::vec4(0.0);
            translation = glm::vec3({0, 0, 5.0});
        }

        camera->setTranslation(translation);
        model->setRotation(glm::mat4(1.0f));
        model->addRotation(glm::vec3{0.0f, 1.0f, 0.0f}, glm::radians(rotation.y));
        model->addRotation(glm::vec3{1.0f, 0.0f, 0.0f}, glm::radians(rotation.x));
        root.setToUpdate();

        renderer.updateUniforms();
        renderer.render();
    }

    std::vector<std::string> names;
    names.reserve(objects.size());
    std::transform(objects.begin(), objects.end(), std::back_inserter(names), [](const auto& object){
        return object->name();
    });
    renderer.unload(names);

    return 0;
}