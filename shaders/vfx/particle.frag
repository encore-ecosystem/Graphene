#version 450
layout(location=0) in vec2 uv;
layout(location=1) in vec4 tint;
layout(location=2) in vec2 motion;
layout(location=3) in float clipW;
layout(location=0) out vec4 color;
layout(location=1) out vec4 outputMotion;
void main(){float radius=length(uv*2-1);float alpha=tint.a*(1-smoothstep(.2,1,radius));if(alpha<.001)discard;color=vec4(tint.rgb,alpha);outputMotion=vec4(motion*.5,clipW,0);}
