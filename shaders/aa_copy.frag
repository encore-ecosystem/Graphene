#version 450
#include "aa_common.glsl"
void main() { output_color = vec4(encodeColor(texture(source_color,bounded(uv)).rgb),1); }
