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

//Material level data
layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outUv;
layout(location = 2) out vec3 outCameraPos;
layout(location = 3) out vec3 outWorldPos;
layout(location = 4) out vec3 outPos;
layout(location = 5) out mat4 outViewMatrix;

void main() {
    outPos = vec3(constants.view * model * vec4(inPosition, 1.0));
    outWorldPos = vec3(model * vec4(inPosition, 1.0));

    outCameraPos = cameraPosition;
    outNormal = (transpose(inverse(constants.view * model)) * vec4(inNormal, 0.0)).xyz;
    outUv = inTexcoord_1;
    outViewMatrix = constants.view;

    gl_Position = constants.projection * constants.view * model * vec4(inPosition, 1.0);
}