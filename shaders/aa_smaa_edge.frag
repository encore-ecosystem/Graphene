#version 450
#include "aa_common.glsl"
#define SMAA_GLSL_4
#define SMAA_PRESET_HIGH
#define SMAA_RT_METRICS aa.metrics
#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
#include "third_party/smaa/SMAA.hlsl"
// Detection uses perceptual RGB; area blending reads linear scene color.
void main() {
    vec4 offsets[3];
    offsets[0]=uv.xyxy+aa.metrics.xyxy*vec4(-1,0,0,-1);
    offsets[1]=uv.xyxy+aa.metrics.xyxy*vec4(1,0,0,1);
    offsets[2]=uv.xyxy+aa.metrics.xyxy*vec4(-2,0,0,-2);
    output_color=vec4(SMAALumaEdgeDetectionPS(uv,offsets,source_color),0,0);
}
