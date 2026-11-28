#ifndef GYO_RENDER_LINEAR_COLOUR_INCLUDED
#define GYO_RENDER_LINEAR_COLOUR_INCLUDED
// The 2D and 3D passes on a linear target (ColourSpace::Linear; the C++ side is include/engine/render/ShaderAbi.hpp's
// LinearColourUniform, Design Invariant RD-4): every colour is taken to linear light before it is used. Fragment
// uniform slot 0 (b0, space3).
#include "Color.hlsli"

cbuffer LinearColour : register(b0, space3)
{
    // x: 1 when the draw's texture samples as linear light (an sRGB texture, a linear target), else 0 (bytes).
    float4 linearColour;
};

// The texel and the colour (bytes, Color8) as linear light, multiplied; alpha as it is.
float4 LinearTimes(float4 texel, float4 colour)
{
    const float3 light = linearColour.x > 0.5f ? texel.rgb : SrgbToLinear(texel.rgb);
    return float4(light * SrgbToLinear(colour.rgb), texel.a * colour.a);
}
#endif
