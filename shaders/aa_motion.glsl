#ifdef GRAPHENE_TEMPORAL
layout(location=13) in vec4 current_clip;
layout(location=14) in vec4 previous_clip;
layout(location=15) flat in float temporal_valid;
layout(location=1) out vec4 output_motion;
void writeMotion() {
    vec2 nowUV=current_clip.xy/current_clip.w*.5+.5;
    vec2 oldUV=previous_clip.xy/max(previous_clip.w,1e-6)*.5+.5;
    output_motion=vec4(nowUV-oldUV,current_clip.w,temporal_valid>.5?previous_clip.w:-1);
}
#endif
