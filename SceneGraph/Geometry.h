//
// Created by Kevin on 22/01/2021.
//

#pragma once

#include <string>
#include <vector>
#define TINYOBJLOADER_IMPLEMENTATION
#include "../libs/tinyobjloader/tiny_obj_loader.h"

struct matrices {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
};

struct VertexData {
    glm::vec3 position{0.0, 0.0, 0.0};
    glm::vec3 normal_1{0.0, 0.0, 0.0};
    glm::vec3 color_1{0.0, 0.0, 0.0};
    glm::vec2 texcoord_1{0.0, 0.0};
    glm::vec2 texcoord_2{0.0, 0.0};
};

class Geometry {
private:

    std::vector<uint32_t> mIndices;
    std::vector<VertexData> mVertexData;

public:

    Geometry(std::vector<uint32_t> indices, std::vector<VertexData> vertex_data) :
        mIndices(indices),
        mVertexData(vertex_data) {
    }

    const std::vector<uint32_t>& indices() const {
        return mIndices;
    }

    const std::vector<VertexData>& vertices() const {
        return mVertexData;
    }

};