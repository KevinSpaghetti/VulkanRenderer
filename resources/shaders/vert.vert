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

layout(location = 0) out vec3 normal;
layout(location = 1) out vec2 uv;
layout(location = 2) out vec3 cameraPos;
layout(location = 3) out vec3 worldPos;
layout(location = 4) out vec4 worldFragPos;

void main() {

    gl_Position = projection * view * model * vec4(inPosition, 1.0);
    worldFragPos = model * vec4(inPosition, 1.0);
    worldPos = vec3(view * model * vec4(inPosition, 1.0));
    cameraPos = cameraPosition;
    normal = mat3(transpose(inverse(model))) * inNormal.xyz;
    uv = inTexcoord_1;
}