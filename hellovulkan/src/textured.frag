#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
layout (location = 0) in vec2 uv;
layout (location = 0) out vec4 outputColor;

layout (binding = 0) uniform texture2D colorTexture;
layout (binding = 1) uniform sampler colorSampler;

void main() {
   outputColor = texture (sampler2D (colorTexture, colorSampler), uv);
}
