#shader vertex
#version 330 core
layout (location = 0) in vec2 iPos;
layout (location = 1) in vec2 iTexCoords;
layout (location = 2) in vec4 iColor;
layout (location = 3) in int iTexIndex;

out vec4 vColor;
out vec2 vTexCoords;

uniform mat4 uMVP;

void main()
{
    vColor = iColor;
    vTexCoords = iTexCoords;

    gl_Position = uMVP * vec4(iPos, 0.0, 1.0);
}

#shader frag
#version 330 core
layout (location = 0) out vec4 oColor;

in vec4 vColor;
in vec2 vTexCoords;

uniform sampler2D uTextures[32];

// 0 = raw texel, 1 = rgb forced opaque, 2 = alpha only, 3 = red only.
// An mtsdf atlas hides its true distance field in alpha, so mode 0 alone is hard
// to read; 1 and 2 split the two halves apart. A single-channel bitmap atlas
// samples as (r, 0, 0, 1), so its coverage needs mode 3.
uniform int uViewMode;

void main()
{
    vec4 texColor = texture(uTextures[0], vTexCoords);

    if (uViewMode == 1) oColor = vec4(texColor.rgb, 1.0) * vec4(vColor.rgb, 1.0);
    else if (uViewMode == 2) oColor = vec4(vec3(texColor.a), 1.0);
    else if (uViewMode == 3) oColor = vec4(vec3(texColor.r), 1.0);
    else oColor = texColor * vColor;
}
