#version 450
#include "aa_common.glsl"
float luma(vec3 c) { return dot(c,vec3(0.299,0.587,0.114)); }
vec3 readColor(vec2 p) { return texture(source_color,bounded(p)).rgb; }
// Spatial edge search following Timothy Lottes' FXAA algorithm.
void main() {
    vec2 px=aa.metrics.xy;
    vec3 m=readColor(uv), n=readColor(uv+vec2(0,-px.y)), s=readColor(uv+vec2(0,px.y));
    vec3 w=readColor(uv+vec2(-px.x,0)), e=readColor(uv+vec2(px.x,0));
    float lm=luma(m), ln=luma(n), ls=luma(s), lw=luma(w), le=luma(e);
    float lo=min(lm,min(min(ln,ls),min(lw,le))), hi=max(lm,max(max(ln,ls),max(lw,le)));
    float range=hi-lo;
    if(range<max(0.0312,hi*0.125)) { output_color=vec4(m,1); return; }
    float nw=luma(readColor(uv-px)), ne=luma(readColor(uv+vec2(px.x,-px.y)));
    float sw=luma(readColor(uv+vec2(-px.x,px.y))), se=luma(readColor(uv+px));
    float horizontal=abs(nw-2*ln+ne)+2*abs(lw-2*lm+le)+abs(sw-2*ls+se);
    float vertical=abs(nw-2*lw+sw)+2*abs(ln-2*lm+ls)+abs(ne-2*le+se);
    bool alongX=vertical>=horizontal;
    float a=alongX?ln:lw,b=alongX?ls:le;
    bool negative=abs(a-lm)>=abs(b-lm);
    float gradient=max(abs(a-lm),abs(b-lm)), average=0.5*(lm+(negative?a:b));
    vec2 across=alongX?vec2(0,px.y):vec2(px.x,0);
    if(negative) across=-across;
    vec2 along=alongX?vec2(px.x,0):vec2(0,px.y);
    vec2 center=uv+across*0.5,p=center-along,q=center+along;
    float lp=luma(readColor(p))-average,lq=luma(readColor(q))-average;
    bool doneP=abs(lp)>=gradient*0.25,doneQ=abs(lq)>=gradient*0.25;
    const float steps[10]=float[](1,1,1,1,1.5,2,2,2,4,8);
    for(int i=0;i<10 && !(doneP&&doneQ);++i) {
        if(!doneP) { p-=along*steps[i];lp=luma(readColor(p))-average;doneP=abs(lp)>=gradient*0.25; }
        if(!doneQ) { q+=along*steps[i];lq=luma(readColor(q))-average;doneQ=abs(lq)>=gradient*0.25; }
    }
    float dp=alongX?uv.x-p.x:uv.y-p.y,dq=alongX?q.x-uv.x:q.y-uv.y;
    float edgeOffset=0.5-min(dp,dq)/max(dp+dq,1e-6);
    if(((dp<dq?lp:lq)<0)==((lm-average)<0)) edgeOffset=0;
    float sub=clamp(abs((2*(ln+ls+lw+le)+nw+ne+sw+se)/12-lm)/range,0,1);
    sub=sub*sub*(3-2*sub);sub=sub*sub*0.5;
    output_color=vec4(readColor(uv+across*max(edgeOffset,sub)),1);
}
