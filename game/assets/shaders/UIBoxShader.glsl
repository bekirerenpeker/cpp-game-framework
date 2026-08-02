#shader vertex
#version 330 core
layout (location = 0) in vec2 iPos;
layout (location = 1) in vec2 iLocalPos;
layout (location = 2) in vec2 iHalfSize;
layout (location = 3) in vec4 iUvRect;
layout (location = 4) in vec4 iFillColor;
layout (location = 5) in vec4 iBorderColor;
layout (location = 6) in vec4 iShadowColor;
layout (location = 7) in vec4 iShadowParams;
layout (location = 8) in vec4 iBorderParams;
layout (location = 9) in int iTexIndex;

out vec2 vLocalPos;
flat out vec2 vHalfSize;
flat out vec4 vUvRect;
flat out vec4 vFillColor;
flat out vec4 vBorderColor;
flat out vec4 vShadowColor;
flat out vec4 vShadowParams;
flat out vec4 vBorderParams;
flat out int vTexIndex;

uniform mat4 uMVP;

void main()
{
    vLocalPos = iLocalPos;
    vHalfSize = iHalfSize;
    vUvRect = iUvRect;
    vFillColor = iFillColor;
    vBorderColor = iBorderColor;
    vShadowColor = iShadowColor;
    vShadowParams = iShadowParams;
    vBorderParams = iBorderParams;
    vTexIndex = iTexIndex;

    gl_Position = uMVP * vec4(iPos, 0.0, 1.0);
}

#shader frag
#version 330 core
layout (location = 0) out vec4 oColor;

// Only the local position interpolates; everything else is one value for the whole
// quad, so flat both skips the interpolation and keeps the corner radius exact.
in vec2 vLocalPos;
flat in vec2 vHalfSize;
flat in vec4 vUvRect;          // uvMin.xy, uvMax.xy
flat in vec4 vFillColor;
flat in vec4 vBorderColor;
flat in vec4 vShadowColor;
flat in vec4 vShadowParams;    // xy = offset, z = blur radius, w = corner radius
flat in vec4 vBorderParams;    // x = width, y = dash period, z = dash ratio
flat in int vTexIndex;

const float HALF_PI = 1.5707963;

uniform sampler2D uTextures[32];

vec4 sampleTexture(vec2 uv)
{
    switch (vTexIndex) {
    case  0: return texture(uTextures[ 0], uv);
    case  1: return texture(uTextures[ 1], uv);
    case  2: return texture(uTextures[ 2], uv);
    case  3: return texture(uTextures[ 3], uv);
    case  4: return texture(uTextures[ 4], uv);
    case  5: return texture(uTextures[ 5], uv);
    case  6: return texture(uTextures[ 6], uv);
    case  7: return texture(uTextures[ 7], uv);
    case  8: return texture(uTextures[ 8], uv);
    case  9: return texture(uTextures[ 9], uv);
    case 10: return texture(uTextures[10], uv);
    case 11: return texture(uTextures[11], uv);
    case 12: return texture(uTextures[12], uv);
    case 13: return texture(uTextures[13], uv);
    case 14: return texture(uTextures[14], uv);
    case 15: return texture(uTextures[15], uv);
    case 16: return texture(uTextures[16], uv);
    case 17: return texture(uTextures[17], uv);
    case 18: return texture(uTextures[18], uv);
    case 19: return texture(uTextures[19], uv);
    case 20: return texture(uTextures[20], uv);
    case 21: return texture(uTextures[21], uv);
    case 22: return texture(uTextures[22], uv);
    case 23: return texture(uTextures[23], uv);
    case 24: return texture(uTextures[24], uv);
    case 25: return texture(uTextures[25], uv);
    case 26: return texture(uTextures[26], uv);
    case 27: return texture(uTextures[27], uv);
    case 28: return texture(uTextures[28], uv);
    case 29: return texture(uTextures[29], uv);
    case 30: return texture(uTextures[30], uv);
    case 31: return texture(uTextures[31], uv);
    }
    return vec4(1.0);
}

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

// Arc length along the boundary, walking counter-clockwise from the middle of the
// right edge, where straight is the half extents of the straight runs. Dashes need a
// coordinate that stays continuous across the corners, so the first quadrant is
// measured directly and the other three mirror onto it. Which feature a fragment
// belongs to is decided by how far it overshoots each straight run rather than by
// its raw coordinates, so a zero radius still separates the edges cleanly.
float perimeterCoord(vec2 p, vec2 straight, float radius, float quarter)
{
    vec2 overshoot = abs(p) - straight;

    float t;
    if (overshoot.x > 0.0 && overshoot.y > 0.0)
        t = straight.y + atan(overshoot.y, overshoot.x) * radius;
    else if (overshoot.x >= overshoot.y) t = abs(p.y);
    else t = straight.y + HALF_PI * radius + (straight.x - abs(p.x));

    if (p.x < 0.0 && p.y > 0.0) return 2.0 * quarter - t;
    if (p.x < 0.0) return 2.0 * quarter + t;
    if (p.y < 0.0) return 4.0 * quarter - t;
    return t;
}

// The pattern repeats every period with the first ratio of it painted. The period
// arrives already divided into the perimeter a whole number of times, so the walk
// closes on itself instead of leaving a stub at the seam.
float dashMask(vec2 p, vec2 halfSize, float radius, vec2 dash, float aa)
{
    vec2 straight = max(halfSize - vec2(radius), vec2(0.0));
    float quarter = straight.x + straight.y + HALF_PI * radius;

    float t = mod(perimeterCoord(p, straight, radius, quarter), dash.x);
    float on = dash.x * dash.y;
    return coverage(-min(t, on - t), aa);
}

vec4 over(vec4 dst, vec4 src)
{
    return vec4(mix(dst.rgb, src.rgb, src.a), src.a + dst.a * (1.0 - src.a));
}

void main()
{
    float radius = vShadowParams.w;
    float blur = vShadowParams.z;
    float borderWidth = vBorderParams.x;

    float dist = sdRoundedBox(vLocalPos, vHalfSize, radius);
    // The gradient of the distance itself, so an edge stays one screen pixel wide
    // whatever the camera zoom is -- these are world units, not pixels.
    float aa = max(fwidth(dist), 0.0001);

    float outer = coverage(dist, aa);
    // The border is the band between the outer edge and the same shape inset by the
    // border width, so a zero width collapses the two and leaves a plain fill.
    float inner = coverage(dist + borderWidth, aa);

    vec4 result = vec4(0.0);

    if (vShadowColor.a > 0.0) {
        float shadowDist = sdRoundedBox(vLocalPos - vShadowParams.xy, vHalfSize, radius);
        // A linear ramp across 2 * blur is not a gaussian, but it is the cheap stand-in
        // every UI does, and it fades to exactly nothing at the padded quad's edge.
        float shadow = coverage(shadowDist, max(2.0 * blur, aa));
        // Knocked out under the box the way CSS does it: a translucent background
        // should show what is behind the element, not the element's own shadow.
        result = vec4(vShadowColor.rgb, vShadowColor.a * shadow * (1.0 - outer));
    }

    // The quad is padded out for the shadow, so uv comes off the box's own extents
    // rather than the interpolated corners, clamped for the padding and the aa fringe.
    vec2 uv = clamp(vLocalPos / vHalfSize * 0.5 + 0.5, 0.0, 1.0);
    vec4 fill = vFillColor * sampleTexture(mix(vUvRect.xy, vUvRect.zw, uv));
    result = over(result, vec4(fill.rgb, fill.a * inner));

    if (borderWidth > 0.0 && vBorderColor.a > 0.0) {
        float ring = outer - inner;
        if (vBorderParams.y > 0.0)
            ring *= dashMask(vLocalPos, vHalfSize, radius, vBorderParams.yz, aa);
        result = over(result, vec4(vBorderColor.rgb, vBorderColor.a * ring));
    }

    if (result.a <= 0.0) discard;
    oColor = result;
}
