#version 440
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform Frame {
    mat4 clipCorrection; // the correction QRhi gives for the backend (Vulkan has Y down)
    vec4 outSize;        // the rectangle the picture is drawn in, in pixels of the screen
    vec4 srcSize;        // the texture, in pixels
};
layout(binding = 1) uniform sampler2D tex;

// Sharp bilinear: the picture is scaled by a whole number with hard pixel edges and only the borders between pixels are
// blended, so that it is neither blurry nor uneven at a size that is not a multiple. Needs the sampler to be linear.
void main()
{
    vec2 scale = max(floor(outSize.xy / srcSize.xy), vec2(1.0));
    vec2 px = v_uv * srcSize.xy;
    vec2 base = floor(px);
    vec2 range = 0.5 - 0.5 / scale;
    vec2 center = fract(px) - 0.5;
    vec2 blend = (center - clamp(center, -range, range)) * scale + 0.5;
    fragColor = vec4(texture(tex, (base + blend) / srcSize.xy).rgb, 1.0);
}
