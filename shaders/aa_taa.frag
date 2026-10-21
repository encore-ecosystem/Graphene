#version 450
#include "aa_common.glsl"
vec3 ycocg(vec3 c) { return vec3(dot(c,vec3(.25,.5,.25)),(c.r-c.b)*.5,(-c.r+2*c.g-c.b)*.25); }
vec3 rgb(vec3 c) { return vec3(c.x+c.y-c.z,c.x+c.z,c.x-c.y-c.z); }
void main() {
    vec2 px=aa.metrics.xy;
    vec3 current=texture(source_color,bounded(uv)).rgb;
    vec4 motion=texture(secondary,bounded(uv));
    // Foreground velocity dilation keeps thin moving silhouettes from trailing.
    for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x) {
        vec4 candidate=texture(secondary,bounded(uv+vec2(x,y)*px));
        if(candidate.z>0 && candidate.z<motion.z) motion=candidate;
    }
    // The raster matrices include jitter; history lives on the stable pixel grid.
    // Removing the jitter delta accumulates coverage instead of tracking the jitter.
    motion.xy-=aa.options.zw;
    vec2 oldUV=uv-motion.xy;
    bool valid=aa.options.x>0.5 && motion.w>0 && all(greaterThanEqual(oldUV,px*.5)) && all(lessThanEqual(oldUV,vec2(1)-px*.5));
    vec4 old=texture(history_color,bounded(oldUV));
    valid=valid && abs(old.a-motion.w)<max(0.02, motion.w*0.015);
    vec3 sum=vec3(0),sum2=vec3(0),low=vec3(1e20),high=vec3(-1e20);
    for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x) {
        vec3 c=ycocg(texture(source_color,bounded(uv+vec2(x,y)*px)).rgb);
        sum+=c;sum2+=c*c;low=min(low,c);high=max(high,c);
    }
    vec3 mean=sum/9, sigma=sqrt(max(sum2/9-mean*mean,vec3(0)));
    vec3 clipped=clamp(ycocg(old.rgb),max(low,mean-1.25*sigma),min(high,mean+1.25*sigma));
    float speed=length(motion.xy/px);
    float accumulated=min(0.90,aa.options.y/(aa.options.y+1.0));
    float historyWeight=valid?mix(accumulated,min(accumulated,0.65),clamp(speed/16,0,1)):0;
    vec3 result=mix(current,rgb(clipped),historyWeight);
    output_color=vec4(max(result,vec3(0)),motion.z);
}
