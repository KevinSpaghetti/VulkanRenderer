#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexcoord_1;
layout(location = 4) in vec2 inTexcoord_2;

//Object level data

layout(set = 0, binding = 0) uniform mmodel{ mat4 model; };
layout(set = 0, binding = 1) uniform mview{ mat4 view; };
layout(set = 0, binding = 2) uniform mprojection{ mat4 projection; };
layout(set = 0, binding = 3) uniform mposition { vec3 cameraPosition; };

layout(std430, push_constant) uniform pconstants {
    mat4 view;
    mat4 projection;
} constants;

void main() {
    gl_Position = constants.projection * constants.view * model * vec4(inPosition, 1.0);
}