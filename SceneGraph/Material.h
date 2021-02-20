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
                m_fragment_shader(fragment_shader){}

    std::vector<char> getVertexShader() const {
        return m_vertex_shader;
    }

    std::vector<char> getFragmentShader() const {
        return m_fragment_shader;
    }

    void addUniform(uint32_t location, Uniform u){
        set.uniforms[location] = u;
    }

    UniformSet uniforms() const {
        return set;
    }


};