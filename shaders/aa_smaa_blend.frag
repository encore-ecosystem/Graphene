#version 450
#include "aa_common.glsl"
#define SMAA_GLSL_4
#define SMAA_PRESET_HIGH
#define SMAA_RT_METRICS aa.metrics
#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
#include "third_party/smaa/SMAA.hlsl"
void main() {
    vec4 offset=uv.xyxy+aa.metrics.xyxy*vec4(1,0,0,1);
    vec4 color=SMAANeighborhoodBlendingPS(uv,offset,source_color,secondary);
    output_color=vec4(encodeColor(color.rgb),1);
}
