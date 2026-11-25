#version 440
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform Frame {
    mat4 clipCorrection; // the correction QRhi gives for the backend (Vulkan has Y down)
    vec4 outSize;        // the rectangle the picture is drawn in, in pixels of the screen
    vec4 srcSize;        // the texture, in pixels
};
layout(binding = 1) uniform sampler2D tex;

// A dark line between the lines of the picture, drawn at the resolution of the screen. It fades out when the picture is
// drawn too small for the lines to be told apart.
void main()
{
    float line = v_uv.y * srcSize.y;
    float strength = 0.55 * clamp(outSize.y / srcSize.y - 1.0, 0.0, 1.0);
    float d = fract(line) - 0.5;
    float beam = 1.0 - strength * (2.0 * d) * (2.0 * d);
    vec2 uv = vec2(v_uv.x, (floor(line) + 0.5) / srcSize.y);
    fragColor = vec4(texture(tex, uv).rgb * beam, 1.0);
}
