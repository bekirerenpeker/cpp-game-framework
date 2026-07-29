#shader vertex
#version 330 core
layout (location = 0) in vec2 iPos;
layout (location = 1) in vec2 iLocalPos;
layout (location = 2) in vec2 iHalfSize;
layout (location = 3) in vec4 iFillColor;
layout (location = 4) in vec4 iBorderColor;
layout (location = 5) in float iCornerRadius;
layout (location = 6) in float iBorderWidth;

out vec2 vLocalPos;
out vec2 vHalfSize;
out vec4 vFillColor;
out vec4 vBorderColor;
out float vCornerRadius;
out float vBorderWidth;

uniform mat4 uMVP;

void main()
{
    vLocalPos = iLocalPos;
    vHalfSize = iHalfSize;
    vFillColor = iFillColor;
    vBorderColor = iBorderColor;
    vCornerRadius = iCornerRadius;
    vBorderWidth = iBorderWidth;

    gl_Position = uMVP * vec4(iPos, 0.0, 1.0);
}

#shader frag
#version 330 core
layout (location = 0) out vec4 oColor;

in vec2 vLocalPos;
in vec2 vHalfSize;
in vec4 vFillColor;
in vec4 vBorderColor;
in float vCornerRadius;
in float vBorderWidth;

// Signed distance to a rounded box centred on the origin: negative inside, zero on
// the edge, positive outside. Everything the UI draws is one evaluation of this.
float sdRoundedBox(vec2 p, vec2 halfSize, float radius)
{
    vec2 q = abs(p) - halfSize + radius;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

// Approximate pixel coverage from a distance and its screen-space gradient. A plain
// step would alias on every rounded corner; smoothstep over a fixed width would blur
// differently depending on the rect's size, since these are already pixel units.
float coverage(float dist, float aa)
{
    return clamp(0.5 - dist / aa, 0.0, 1.0);
}

void main()
{
    // Clamped so an over-large radius rounds to a capsule/circle instead of folding
    // the distance field inside out.
    float radius = clamp(vCornerRadius, 0.0, min(vHalfSize.x, vHalfSize.y));
    float dist = sdRoundedBox(vLocalPos, vHalfSize, radius);
    float aa = max(fwidth(dist), 0.0001);

    float outer = coverage(dist, aa);
    if (outer <= 0.0) discard;

    // The border is the band between the outer edge and the same shape inset by the
    // border width, so a zero width collapses the two and leaves a plain fill.
    float inner = coverage(dist + vBorderWidth, aa);
    vec4 color = mix(vBorderColor, vFillColor, inner);

    oColor = vec4(color.rgb, color.a * outer);
}
