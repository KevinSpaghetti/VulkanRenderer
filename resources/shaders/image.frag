#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec3 cameraPos;
layout(location = 3) in vec3 inWorldPos;


layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 1) uniform sampler2D albedo;
layout(set = 1, binding = 2) uniform samplerCube cubemap;

void main() {

    vec3 I = normalize(inWorldPos - cameraPos);
    vec3 R = reflect(I, normalize(inNormal));
    vec3 albedo = texture(cubemap, I).rgb;
    outColor = vec4(albedo, 1.0);

}