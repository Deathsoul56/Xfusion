#version 440
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform Frame {
    mat4 clipCorrection; // the correction QRhi gives for the backend (Vulkan has Y down)
    vec4 outSize;        // the rectangle the picture is drawn in, in pixels of the screen
    vec4 srcSize;        // the texture, in pixels
};
layout(binding = 1) uniform sampler2D tex;

// Scanlines, an aperture grille (red, green and blue stripes) and a little vignette, a flat CRT.
void main()
{
    float line = v_uv.y * srcSize.y;
    float lines = clamp(outSize.y / srcSize.y - 1.0, 0.0, 1.0);
    float d = fract(line) - 0.5;
    float beam = 1.0 - 0.6 * lines * (2.0 * d) * (2.0 * d);
    vec2 uv = vec2(v_uv.x, (floor(line) + 0.5) / srcSize.y);
    vec3 color = texture(tex, uv).rgb * beam;

    float stripes = clamp(outSize.x / srcSize.x - 1.5, 0.0, 1.0);
    float column = mod(floor(v_uv.x * outSize.x), 3.0);
    vec3 grille = vec3(column < 1.0 ? 1.0 : 0.7, (column >= 1.0 && column < 2.0) ? 1.0 : 0.7, column >= 2.0 ? 1.0 : 0.7);
    color *= mix(vec3(1.0), grille * 1.15, stripes);

    vec2 c = v_uv - 0.5;
    color *= 1.0 - 0.35 * dot(c, c);
    fragColor = vec4(color, 1.0);
}
