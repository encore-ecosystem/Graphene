#version 450
#include "aa_common.glsl"
void main() {
    // Sky, clouds, precipitation and transparent VFX mark an invalid history
    // in motion.w. Dilate by one render pixel to protect moving silhouettes.
    vec2 pixel=1./vec2(textureSize(source_color,0));
    float reactive=0.;
    for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x)
        reactive=max(reactive,texture(source_color,clamp(uv+vec2(x,y)*pixel,pixel*.5,1.-pixel*.5)).w<=0.?1.:0.);
    output_color=vec4(reactive,0,0,1);
}
