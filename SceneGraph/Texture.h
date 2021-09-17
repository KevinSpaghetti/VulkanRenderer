//
// Created by Kevin on 22/01/2021.
//

#pragma once

#include <string>
#include <glm/vec2.hpp>
#include <stdexcept>
#include <memory>

struct Sampler{
    int minFilter;
    int magFilter;
    int wrapS;
    int wrapT;
};

class Texture2D {
private:

    glm::ivec2 m_image_size;
    int m_image_channels;

    std::shared_ptr<unsigned char> m_image_data;

public:

    Texture2D() {}
    Texture2D(glm::ivec2 size, int channels,
            unsigned char* data) :
        m_image_size(size),
        m_image_channels(channels),
        m_image_data(data){}

    std::shared_ptr<void> data() const {
        return m_image_data;
    }

    uint32_t data_size() const {
        return m_image_size.x * m_image_size.y * m_image_channels;
    }

    glm::ivec2 size() const {
        return m_image_size;
    }

};