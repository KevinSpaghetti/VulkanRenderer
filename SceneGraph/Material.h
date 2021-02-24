//
// Created by Kevin on 22/01/2021.
//

#pragma once

#include <string>
#include <map>
#include <fstream>
#include "Texture.h"
#include "../Vulkan/Resources.h"

class Material {
private:
    std::string m_name;

    const std::vector<char> m_vertex_shader;
    const std::vector<char> m_fragment_shader;

    //Material input data
    UniformSet set;

public:

    Material(const std::string name,
            const std::vector<char> vertex_shader,
            const std::vector<char> fragment_shader) :
                m_name(name),
                m_vertex_shader(vertex_shader),
                m_fragment_shader(fragment_shader){
        set = getMaterialSetArchetype();
    }

    std::vector<char> getVertexShader() const {
        return m_vertex_shader;
    }

    std::vector<char> getFragmentShader() const {
        return m_fragment_shader;
    }

    static UniformSet getMaterialSetArchetype(){
        UniformSet materialSetArchetype;
        materialSetArchetype.slot = 1;
        std::shared_ptr<unsigned char> albedoDataPtr(new unsigned char[1024 * 1024 * 4]);
        std::shared_ptr<unsigned char> normalDataPtr(new unsigned char[1024 * 1024 * 4]);
        std::shared_ptr<unsigned char> specularDataPtr(new unsigned char[1024 * 1024 * 4]);
        std::shared_ptr<unsigned char> emissiveDataPtr(new unsigned char[1024 * 1024 * 4]);

        materialSetArchetype.uniforms[0] = Uniform{.type = TYPE_IMAGE, .size = {1024, 1024, 0},
                .byte_size = 1024 * 1024 * 4, .count = 1,
                .data = albedoDataPtr
        };
        materialSetArchetype.uniforms[1] = Uniform{.type = TYPE_IMAGE, .size = {1024, 1024, 0},
                .byte_size = 1024 * 1024 * 4, .count = 1,
                .data = normalDataPtr
        };
        materialSetArchetype.uniforms[2] = Uniform{.type = TYPE_IMAGE, .size = {1024, 1024, 0},
                .byte_size = 1024 * 1024 * 4, .count = 1,
                .data = specularDataPtr
        };
        materialSetArchetype.uniforms[3] = Uniform{.type = TYPE_IMAGE, .size = {1024, 1024, 0},
                .byte_size = 1024 * 1024 * 4, .count = 1,
                .data = emissiveDataPtr
        };

        return materialSetArchetype;
    }

    Uniform& uniform(uint32_t location){
        return set.uniforms[location];
    }

    UniformSet uniforms() const {
        return set;
    }


};