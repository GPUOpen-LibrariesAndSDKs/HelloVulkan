#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 inUv;
layout (location = 0) out vec2 outUv;
void main() {
    gl_Position = vec4(pos,1);
    outUv = inUv;
}
