#version 440
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform Frame {
    mat4 clipCorrection; // the correction QRhi gives for the backend (Vulkan has Y down)
    vec4 outSize;        // the rectangle the picture is drawn in, in pixels of the screen
    vec4 srcSize;        // the texture, in pixels
};
layout(binding = 1) uniform sampler2D tex;

// A grid of dark gaps between the pixels of the picture, like an LCD (the Game Gear). Fades out when the pixels are drawn
// too small for it.
void main()
{
    vec2 px = v_uv * srcSize.xy;
    vec2 f = fract(px);
    vec2 scale = outSize.xy / srcSize.xy;
    float amount = clamp((min(scale.x, scale.y) - 2.0) / 2.0, 0.0, 1.0);
    vec2 inside = smoothstep(0.0, 0.14, f) * smoothstep(0.0, 0.14, 1.0 - f);
    float grid = mix(1.0, inside.x * inside.y, amount);
    vec3 color = texture(tex, (floor(px) + 0.5) / srcSize.xy).rgb;
    fragColor = vec4(color * grid * mix(1.0, 1.12, amount), 1.0);
}
