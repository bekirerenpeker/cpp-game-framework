#shader vertex
#version 330 core
layout (location = 0) in vec2 iPos;
layout (location = 1) in vec2 iTexCoords;
layout (location = 2) in vec4 iColor;
layout (location = 3) in int iTexIndex;

out vec4 vColor;
out vec2 vTexCoords;
flat out int vTexIndex;

uniform mat4 uMVP;

void main()
{
    vColor = iColor;
    vTexCoords = iTexCoords;
    vTexIndex = iTexIndex;

    gl_Position = uMVP * vec4(iPos, 0.0, 1.0);
}

#shader frag
#version 330 core
layout (location = 0) out vec4 oColor;

in vec4 vColor;
in vec2 vTexCoords;
flat in int vTexIndex;

uniform sampler2D uTextures[32];

void main()
{
    vec4 texColor = vec4(1.0);

    switch(vTexIndex) {
    case  0: texColor = texture(uTextures[ 0], vTexCoords); break;
    case  1: texColor = texture(uTextures[ 1], vTexCoords); break;
    case  2: texColor = texture(uTextures[ 2], vTexCoords); break;
    case  3: texColor = texture(uTextures[ 3], vTexCoords); break;
    case  4: texColor = texture(uTextures[ 4], vTexCoords); break;
    case  5: texColor = texture(uTextures[ 5], vTexCoords); break;
    case  6: texColor = texture(uTextures[ 6], vTexCoords); break;
    case  7: texColor = texture(uTextures[ 7], vTexCoords); break;
    case  8: texColor = texture(uTextures[ 8], vTexCoords); break;
    case  9: texColor = texture(uTextures[ 9], vTexCoords); break;
    case 10: texColor = texture(uTextures[10], vTexCoords); break;
    case 11: texColor = texture(uTextures[11], vTexCoords); break;
    case 12: texColor = texture(uTextures[12], vTexCoords); break;
    case 13: texColor = texture(uTextures[13], vTexCoords); break;
    case 14: texColor = texture(uTextures[14], vTexCoords); break;
    case 15: texColor = texture(uTextures[15], vTexCoords); break;
    case 16: texColor = texture(uTextures[16], vTexCoords); break;
    case 17: texColor = texture(uTextures[17], vTexCoords); break;
    case 18: texColor = texture(uTextures[18], vTexCoords); break;
    case 19: texColor = texture(uTextures[19], vTexCoords); break;
    case 20: texColor = texture(uTextures[20], vTexCoords); break;
    case 21: texColor = texture(uTextures[21], vTexCoords); break;
    case 22: texColor = texture(uTextures[22], vTexCoords); break;
    case 23: texColor = texture(uTextures[23], vTexCoords); break;
    case 24: texColor = texture(uTextures[24], vTexCoords); break;
    case 25: texColor = texture(uTextures[25], vTexCoords); break;
    case 26: texColor = texture(uTextures[26], vTexCoords); break;
    case 27: texColor = texture(uTextures[27], vTexCoords); break;
    case 28: texColor = texture(uTextures[28], vTexCoords); break;
    case 29: texColor = texture(uTextures[29], vTexCoords); break;
    case 30: texColor = texture(uTextures[30], vTexCoords); break;
    case 31: texColor = texture(uTextures[31], vTexCoords); break;
    }     

    oColor = vColor * texColor * vec4(1,1,1,0.9);
}