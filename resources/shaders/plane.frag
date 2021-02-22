#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec3 cameraPos;
layout(location = 3) in vec3 inWorldPos;
layout(location = 4) in vec4 lightFragPos;
layout(location = 5) in vec4 FragPos;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D shadowMap1;
layout(set = 1, binding = 1) uniform sampler2D shadowMap2;
layout(set = 1, binding = 2) uniform sampler2D shadowMap3;

void main() {
    const float bias = 0.005;
    vec3 projCoords = lightFragPos.xyz / lightFragPos.w;
    vec2 coords = projCoords.xy * 0.5 + 0.5;
    float closestDepth = texture(shadowMap2, coords).x;
    float currentDepth = projCoords.z;
    if((pow(coords.x - 0.5, 2) + pow(coords.y - 0.5, 2) < pow(0.5, 2)) && currentDepth < closestDepth){
        outColor = vec4(1.0, 1.0, 1.0, 1.0);
    }else{
        outColor = vec4(0.1, 0.1, 0.1, 1.0);
    }

}