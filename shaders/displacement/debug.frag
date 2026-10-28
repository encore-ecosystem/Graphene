#version 450
layout(location=0) in vec3 world_normal;
layout(location=0) out vec4 output_color;
void main() { output_color=vec4(abs(normalize(world_normal))*.5+vec3(.1,.35,.1),1); }
