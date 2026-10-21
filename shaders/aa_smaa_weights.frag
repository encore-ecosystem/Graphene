#version 450
#include "aa_common.glsl"
#define SMAA_GLSL_4
#define SMAA_PRESET_HIGH
#define SMAA_RT_METRICS aa.metrics
#define SMAA_INCLUDE_VS 1
#define SMAA_INCLUDE_PS 1
#include "third_party/smaa/SMAA.hlsl"
void main() {
    vec2 pixcoord;vec4 offsets[3];
    SMAABlendingWeightCalculationVS(uv,pixcoord,offsets);
    output_color=SMAABlendingWeightCalculationPS(uv,pixcoord,offsets,source_color,area_texture,search_texture,vec4(0));
}
