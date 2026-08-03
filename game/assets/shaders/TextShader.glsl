#shader vertex
#version 330 core
layout (location = 0) in vec2 iPos;
layout (location = 1) in vec2 iTexCoords;
layout (location = 2) in vec4 iClipRect;
layout (location = 3) in vec4 iColor;
layout (location = 4) in vec4 iOutlineColor;
layout (location = 5) in vec4 iShadowColor;
layout (location = 6) in vec2 iUnitRange;
layout (location = 7) in vec4 iShadowParams;
layout (location = 8) in vec4 iParams;
layout (location = 9) in int iTexIndex;

out vec2 vPos;
flat out vec4 vClipRect;
out vec4 vColor;
out vec4 vOutlineColor;
out vec4 vShadowColor;
out vec2 vTexCoords;
out vec2 vUnitRange;
out vec4 vShadowParams;
out vec4 vParams;
flat out int vTexIndex;

uniform mat4 uMVP;

void main()
{
    vPos = iPos;
    vClipRect = iClipRect;
    vColor = iColor;
    vOutlineColor = iOutlineColor;
    vShadowColor = iShadowColor;
    vTexCoords = iTexCoords;
    vUnitRange = iUnitRange;
    vShadowParams = iShadowParams;
    vParams = iParams;
    vTexIndex = iTexIndex;

    gl_Position = uMVP * vec4(iPos, 0.0, 1.0);
}

#shader frag
#version 330 core
layout (location = 0) out vec4 oColor;

in vec2 vPos;
flat in vec4 vClipRect;   // minX, minY, maxX, maxY, same space as vPos
in vec4 vColor;
in vec4 vOutlineColor;
in vec4 vShadowColor;
in vec2 vTexCoords;
in vec2 vUnitRange;
in vec4 vShadowParams;   // xy = shadow uv offset, z = shadow softness, w = unused
in vec4 vParams;         // outline width, boldness, softness, shadow width
flat in int vTexIndex;

uniform sampler2D uTextures[32];

vec4 sampleAtlas(vec2 uv)
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

float median(vec3 v)
{
    return max(min(v.r, v.g), min(max(v.r, v.g), v.b));
}

// Converts the atlas's distance range into screen pixels for this fragment.
// Derived from a per-vertex unitRange plus fwidth rather than textureSize(),
// because GLSL 330 won't reliably index a sampler array dynamically.
float screenPxRange()
{
    vec2 screenTexSize = vec2(1.0) / fwidth(vTexCoords);
    return max(0.5 * dot(vUnitRange, screenTexSize), 1.0);
}

// The field only carries usable distance within MAX_FIELD_OFFSET of the edge, so
// every transition has to finish inside that band.
const float MAX_FIELD_OFFSET = 0.45;

// Coverage for one layer, worked out in normalized field units so it scales with
// the text rather than the zoom. field is the raw sample (0.5 == glyph edge),
// edgeOffset pushes the layer outwards (boldness, outline or glow width) and
// softNorm is the transition width.
//
// Both the softness floor and the offset clamp matter: the floor keeps a crisp edge
// one screen pixel wide instead of aliasing, and the clamp guarantees the ramp's
// lower bound stays above zero, so a wide glow fades to *exactly* nothing before it
// reaches the glyph quad's edge instead of leaving a faint tint across the quad.
float layerAlpha(float field, float edgeOffset, float softNorm, float pxRange)
{
    float minSoft = 1.0 / max(pxRange, 0.0001);
    float soft = clamp(softNorm, minSoft, 2.0 * MAX_FIELD_OFFSET);
    float offset = min(edgeOffset, MAX_FIELD_OFFSET - soft * 0.5);
    float edge = 0.5 - offset;
    return smoothstep(edge - soft * 0.5, edge + soft * 0.5, field);
}

vec4 over(vec4 dst, vec4 src)
{
    float a = src.a + dst.a * (1.0 - src.a);
    vec3 rgb = mix(dst.rgb, src.rgb, src.a);
    return vec4(rgb, a);
}

void main()
{
    // The accumulated box of every clipping ancestor, so a glyph half inside a scroll
    // container is cut mid-letter. World-space text carries a rect wide enough that
    // this never rejects, which is why it needs no branch of its own.
    if (vPos.x < vClipRect.x || vPos.y < vClipRect.y || vPos.x > vClipRect.z ||
        vPos.y > vClipRect.w)
        discard;

    // A negative unitRange means "ignore the atlas and fill flat": underline,
    // strikethrough, and any plain UI rect later.
    if (vUnitRange.x < 0.0) {
        oColor = vColor;
        return;
    }

    // A zero unitRange means this is not a distance field: a single-channel bitmap
    // atlas with coverage in .r. Every distance-field effect is inapplicable here,
    // which is exactly how a Bitmap font ends up ignoring them.
    if (vUnitRange == vec2(0.0)) {
        vec4 texel = sampleAtlas(vTexCoords);
        oColor = vec4(vColor.rgb, vColor.a * texel.r);
        return;
    }

    // All widths arrive as normalized field units, where 1.0 spans the whole baked
    // distance range, so every effect stays proportional to the text under zoom.
    float outlineWidth = max(vParams.x, 0.0);
    float boldness = vParams.y;
    float softness = max(vParams.z, 0.0);
    float shadowWidth = max(vParams.w, 0.0);
    vec2 shadowOffset = vShadowParams.xy;
    float shadowSoftness = max(vShadowParams.z, 0.0);

    float pxRange = screenPxRange();

    // The sharp multi-channel median drives the fill and the outline so the two
    // agree at corners; the shadow uses mtsdf's true SDF in .a, which is smooth
    // and so behaves better when it is offset and blurred.
    float field = median(sampleAtlas(vTexCoords).rgb);

    float fillAlpha = layerAlpha(field, boldness, softness, pxRange);
    vec4 result = vec4(0.0);

    bool hasShadow = vShadowColor.a > 0.0 && (shadowWidth > 0.0 || shadowOffset != vec2(0.0));
    if (hasShadow) {
        float shadowField = sampleAtlas(vTexCoords + shadowOffset).a;
        float shadowAlpha =
            layerAlpha(shadowField, boldness + shadowWidth, shadowSoftness, pxRange);
        result = vec4(vShadowColor.rgb, vShadowColor.a * shadowAlpha);
    }

    if (vOutlineColor.a > 0.0 && outlineWidth > 0.0) {
        float outlineAlpha = layerAlpha(field, boldness + outlineWidth, softness, pxRange);
        result = over(result, vec4(vOutlineColor.rgb, vOutlineColor.a * outlineAlpha));
    }

    result = over(result, vec4(vColor.rgb, vColor.a * fillAlpha));

    oColor = result;
}
