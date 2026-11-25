#version 440
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform Frame {
    mat4 clipCorrection; // the correction QRhi gives for the backend (Vulkan has Y down)
    vec4 outSize;        // the rectangle the picture is drawn in, in pixels of the screen
    vec4 srcSize;        // the texture, in pixels
};
layout(binding = 1) uniform sampler2D tex;

void main()
{
    fragColor = vec4(texture(tex, v_uv).rgb, 1.0);
}
