#shader vertex
#version 330 core
layout (location = 0) in vec2 iPos;
layout (location = 1) in vec2 iLocalPos;
layout (location = 2) in vec2 iHalfSize;
layout (location = 3) in vec4 iUvRect;
layout (location = 4) in vec4 iClipRect;
layout (location = 5) in vec4 iParams0;
layout (location = 6) in vec4 iParams1;
layout (location = 7) in vec4 iUnused0;
layout (location = 8) in vec4 iUnused1;
layout (location = 9) in vec4 iUnused2;
layout (location = 10) in vec4 iUnused3;
layout (location = 11) in int iTexIndex;

out vec2 vLocalPos;
out vec2 vPos;
flat out vec2 vHalfSize;
flat out vec4 vClipRect;
flat out vec4 vParams0;
flat out vec4 vParams1;

uniform mat4 uMVP;

void main()
{
    vLocalPos = iLocalPos;
    vPos = iPos;
    vHalfSize = iHalfSize;
    vClipRect = iClipRect;
    vParams0 = iParams0;
    vParams1 = iParams1;

    gl_Position = uMVP * vec4(iPos, 0.0, 1.0);
}

#shader frag
#version 330 core
layout (location = 0) out vec4 oColor;

in vec2 vLocalPos;
in vec2 vPos;
flat in vec2 vHalfSize;
flat in vec4 vClipRect;
flat in vec4 vParams0;   // x = mode (0 sat/val square, 1 hue strip, 2 alpha strip), y = hue
flat in vec4 vParams1;   // rgb = the colour an alpha strip fades in

const float CHECKER_SIZE = 6.0;

vec3 hueToRgb(float h)
{
    return clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
}

void main()
{
    if (vPos.x < vClipRect.x || vPos.y < vClipRect.y || vPos.x > vClipRect.z ||
        vPos.y > vClipRect.w)
        discard;

    // Local position is centre-origin and y-up; uv is top-left origin so the bright
    // end of both controls sits at the top the way every colour picker draws it.
    vec2 uv = vec2(
        (vLocalPos.x + vHalfSize.x) / (vHalfSize.x * 2.0),
        1.0 - (vLocalPos.y + vHalfSize.y) / (vHalfSize.y * 2.0)
    );

    if (vParams0.x < 0.5) {
        vec3 hue = hueToRgb(vParams0.y);
        vec3 color = mix(vec3(1.0), hue, uv.x);
        color *= 1.0 - uv.y;
        oColor = vec4(color, 1.0);
    } else if (vParams0.x < 1.5) {
        oColor = vec4(hueToRgb(uv.y), 1.0);
    } else {
        // Composited against a checkerboard rather than drawn translucent, or the strip
        // reads as a gradient of the panel behind it instead of one of transparency.
        vec2 cell = floor(gl_FragCoord.xy / CHECKER_SIZE);
        float check = mod(cell.x + cell.y, 2.0);
        vec3 backdrop = mix(vec3(0.32), vec3(0.52), check);
        oColor = vec4(mix(backdrop, vParams1.rgb, uv.x), 1.0);
    }
}
