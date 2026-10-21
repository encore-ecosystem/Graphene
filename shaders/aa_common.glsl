layout(set=0,binding=0) uniform Params { vec4 metrics; vec4 options; } aa;
layout(set=0,binding=1) uniform sampler2D source_color;
layout(set=0,binding=2) uniform sampler2D secondary;
layout(set=0,binding=3) uniform sampler2D history_color;
layout(set=0,binding=4) uniform sampler2D area_texture;
layout(set=0,binding=5) uniform sampler2D search_texture;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 output_color;
vec3 encodeColor(vec3 c) {
    c = max(c, vec3(0));
    return mix(12.92*c, 1.055*pow(c,vec3(1.0/2.4))-0.055, greaterThan(c,vec3(0.0031308)));
}
vec2 bounded(vec2 p) { return clamp(p, aa.metrics.xy * 0.5, vec2(1.0) - aa.metrics.xy * 0.5); }
