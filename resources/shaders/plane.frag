#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec3 cameraPos;
layout(location = 3) in vec3 inWorldPos;
layout(location = 4) in vec4 worldFragPosition;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D albedoTexture;
layout(set = 1, binding = 1) uniform sampler2D normalTexture;
layout(set = 1, binding = 2) uniform sampler2D specularTexture;
layout(set = 1, binding = 3) uniform sampler2D emissiveTexture;

layout(set = 2, binding = 0) buffer _ { mat4 lightMatrices[]; };
layout(set = 2, binding = 1) uniform sampler2D shadowMaps[];

const float lightStrength = 0.8;

void main() {

    outColor = vec4(0.1, 0.1, 0.1, 1.0);
    for(int i = 0; i < lightMatrices.length(); ++i){
        vec4 lightFragPos = lightMatrices[i] * worldFragPosition;
        vec3 projCoords = lightFragPos.xyz / lightFragPos.w;
        vec2 coords = projCoords.xy * 0.5 + 0.5;
        float closestDepth = texture(shadowMaps[i], coords).x;
        float currentDepth = projCoords.z;
        if((pow(coords.x - 0.5, 2) + pow(coords.y - 0.5, 2) < pow(0.5, 2)) && currentDepth < closestDepth){
            outColor += vec4(lightStrength, lightStrength, lightStrength, 1.0);
        }
    }
}