#ifndef GYO_RENDER_TONEMAPPING_METHODS_H
#define GYO_RENDER_TONEMAPPING_METHODS_H
/* The tonemapping methods' values (render-pipeline declaration section 16), one list for both sides of the shader ABI
   (Design Invariant RD-4): Drawables.hpp's Tonemapping::Method takes its values from here and shaders/Tonemapping.hlsli
   includes it. Plain #defines and a C comment: both C++'s and HLSL's preprocessors read this file. */
#define GYO_TONEMAP_NONE 1
#define GYO_TONEMAP_REINHARD 2
#define GYO_TONEMAP_REINHARD_LUMINANCE 3
#define GYO_TONEMAP_ACES_FITTED 4
#define GYO_TONEMAP_SOMEWHAT_BORING_DISPLAY_TRANSFORM 5
#define GYO_TONEMAP_KHRONOS_PBR_NEUTRAL 6
#endif
