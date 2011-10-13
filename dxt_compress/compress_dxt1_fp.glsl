#version 130

#extension GL_EXT_gpu_shader4 : enable

#define lerp mix

// Image formats
const int FORMAT_RGB = 0;
const int FORMAT_YUV = 1;

// Covert YUV to RGB
vec3 ConvertYUVToRGB(vec3 color)
{
    float Y = color[0];
    float U = color[1] - 0.5;
    float V = color[2] - 0.5;
    Y = 1.1643 * (Y - 0.0625);

    float R = Y + 1.5958 * V;
    float G = Y - 0.39173 * U - 0.81290 * V;
    float B = Y + 2.017 * U;
    
    return vec3(R, G, B);
}

float colorDistance(vec3 c0, vec3 c1)
{
    return dot(c0-c1, c0-c1);
}
float colorDistance(vec2 c0, vec2 c1)
{
    return dot(c0-c1, c0-c1);
}

void ExtractColorBlockRGB(out vec3 col[16], sampler2D image, vec4 texcoord, vec2 imageSize)
{
    vec2 texelSize = (1.0f / imageSize);
    vec2 tex = vec2(texcoord.x, texcoord.y);
    tex -= texelSize * vec2(2);
    for ( int i = 0; i < 4; i++ ) {
        for ( int j = 0; j < 4; j++ ) {
            col[i * 4 + j] = texture(image, tex + vec2(j, i) * texelSize).rgb;
        }
    }
}

void ExtractColorBlockYUV(out vec3 col[16], sampler2D image, vec4 texcoord, vec2 imageSize)
{
    vec2 texelSize = (1.0f / imageSize);
    vec2 tex = vec2(texcoord.x, texcoord.y);
    tex -= texelSize * vec2(2);
    for ( int i = 0; i < 4; i++ ) {
        for ( int j = 0; j < 4; j++ ) {
            col[i * 4 + j] = ConvertYUVToRGB(texture(image, tex + vec2(j, i) * texelSize).rgb);
        }
    }
}

void FindMinMaxColorsBox(vec3 block[16], out vec3 mincol, out vec3 maxcol)
{
    mincol = block[0];
    maxcol = block[0];
    
    for ( int i = 1; i < 16; i++ ) {
        mincol = min(mincol, block[i]);
        maxcol = max(maxcol, block[i]);
    }
}

void SelectDiagonal(vec3 block[16], inout vec3 mincol, inout vec3 maxcol)
{
    vec3 center = (mincol + maxcol) * 0.5;

    vec2 cov = vec2(0, 0);
    for (int i = 0; i < 16; i++) {
        vec3 t = block[i] - center;
        cov.x += t.x * t.z;
        cov.y += t.y * t.z;
    }

    if (cov.x < 0) {
        float temp = maxcol.x;
        maxcol.x = mincol.x;
        mincol.x = temp;
    }
    if (cov.y < 0) {
        float temp = maxcol.y;
        maxcol.y = mincol.y;
        mincol.y = temp;
    }
}

void InsetBBox(inout vec3 mincol, inout vec3 maxcol)
{
    vec3 inset = (maxcol - mincol) / 16.0 - (8.0 / 255.0) / 16;
    mincol = clamp(mincol + inset, 0.0, 1.0);
    maxcol = clamp(maxcol - inset, 0.0, 1.0);
}

vec3 RoundAndExpand(vec3 v, out uint w)
{
    uvec3 c = uvec3(round(v * vec3(31, 63, 31)));
    w = (c.r << 11u) | (c.g << 5u) | c.b;

    c.rb = (c.rb << 3u) | (c.rb >> 2u);
    c.g = (c.g << 2u) | (c.g >> 4u);

    return vec3(c) * (1.0 / 255.0);
}

uint EmitEndPointsDXT1(inout vec3 mincol, inout vec3 maxcol)
{
    uvec2 outp;
    maxcol = RoundAndExpand(maxcol, outp.x);
    mincol = RoundAndExpand(mincol, outp.y);

    // We have to do this in case we select an alternate diagonal.
    if (outp.x < outp.y)
    {
        vec3 tmp = mincol;
        mincol = maxcol;
        maxcol = tmp;
        return outp.y | (outp.x << 16u);
    }

    return outp.x | (outp.y << 16u);
}

uint EmitIndicesDXT1(vec3 col[16], vec3 mincol, vec3 maxcol)
{
    // Compute palette
    vec3 c[4];
    c[0] = maxcol;
    c[1] = mincol;
    c[2] = lerp(c[0], c[1], 1.0/3.0);
    c[3] = lerp(c[0], c[1], 2.0/3.0);

    // Compute indices
    uint indices = 0u;
    for ( int i = 0; i < 16; i++ ) {

        // find index of closest color
        vec4 dist;
        dist.x = colorDistance(col[i], c[0]);
        dist.y = colorDistance(col[i], c[1]);
        dist.z = colorDistance(col[i], c[2]);
        dist.w = colorDistance(col[i], c[3]);
        
        uvec4 b;
        b.x = dist.x > dist.w ? 1u : 0u;
        b.y = dist.y > dist.z ? 1u : 0u;
        b.z = dist.x > dist.z ? 1u : 0u;
        b.w = dist.y > dist.w ? 1u : 0u;
        uint b4 = dist.z > dist.w ? 1u : 0u;
        
        uint index = (b.x & b4) | (((b.y & b.z) | (b.x & b.w)) << 1u);
        indices |= index << (uint(i) * 2u);
    }

    // Output indices
    return indices;
}

in vec4 TEX0;
uniform sampler2D image;
uniform int imageFormat = FORMAT_RGB;
uniform vec2 imageSize;
out uvec4 colorInt;

void main()
{
    // Read block
    vec3 block[16];
    if ( int(imageFormat) == FORMAT_YUV )
        ExtractColorBlockYUV(block, image, TEX0, imageSize);
    else
        ExtractColorBlockRGB(block, image, TEX0, imageSize);

    // Find min and max colors
    vec3 mincol, maxcol;
    FindMinMaxColorsBox(block, mincol, maxcol);

    SelectDiagonal(block, mincol, maxcol);

    InsetBBox(mincol, maxcol);

    uvec4 outp;
    outp.x = EmitEndPointsDXT1(mincol, maxcol);
    outp.w = EmitIndicesDXT1(block, mincol, maxcol);
    outp.y = 0u;
    outp.z = 0u;

    colorInt = outp;
}