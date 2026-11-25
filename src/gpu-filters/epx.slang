#version 450

// EPX (Eric's Pixel Expansion), scaling by two: every pixel becomes four, and a corner takes the colour of a neighbour when
// the two neighbours on that corner agree with each other and not with the opposite ones. Written for Xfusion from the
// description of the algorithm.
layout(push_constant) uniform Push
{
    vec4 SourceSize;
    vec4 OriginalSize;
    vec4 OutputSize;
    uint FrameCount;
} params;

layout(std140, set = 0, binding = 0) uniform UBO
{
    mat4 MVP;
} global;

#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;

void main()
{
    gl_Position = global.MVP * Position;
    vTexCoord = TexCoord;
}

#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D Source;

vec3 pixel(ivec2 at)
{
    ivec2 size = textureSize(Source, 0);
    return texelFetch(Source, clamp(at, ivec2(0), size - 1), 0).rgb;
}

void main()
{
    vec2 position = vTexCoord * params.SourceSize.xy;
    ivec2 at = ivec2(floor(position));
    vec2 inside = fract(position);

    vec3 p = pixel(at);
    vec3 up = pixel(at + ivec2(0, -1));
    vec3 right = pixel(at + ivec2(1, 0));
    vec3 left = pixel(at + ivec2(-1, 0));
    vec3 down = pixel(at + ivec2(0, 1));

    vec3 result = p;
    if (inside.x < 0.5 && inside.y < 0.5) {
        if (left == up && left != down && up != right)
            result = up;
    } else if (inside.x >= 0.5 && inside.y < 0.5) {
        if (up == right && up != left && right != down)
            result = right;
    } else if (inside.x < 0.5) {
        if (down == left && down != right && left != up)
            result = left;
    } else {
        if (right == down && right != up && down != left)
            result = down;
    }
    FragColor = vec4(result, 1.0);
}
