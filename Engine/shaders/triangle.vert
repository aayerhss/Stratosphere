#version 450

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inInstanceOffset;
layout(location = 3) in vec3 inInstanceColor;

layout(push_constant) uniform Push {
    vec2 offset;
} pc;

layout(location = 0) out vec3 fragColor;

void main() {
    vec2 pos = inPos + pc.offset + inInstanceOffset;
    gl_Position = vec4(pos, 0.0, 1.0);
    // Instance color drives the final shade (vertex color kept for compatibility)
    fragColor = inInstanceColor;
}