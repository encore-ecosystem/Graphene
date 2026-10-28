#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=3) in vec2 uv;
layout(set=0,binding=0) uniform Preview {mat4 vp;vec4 camera;vec4 options;} frame;
layout(location=0) out vec3 worldPosition;
layout(location=1) out vec3 worldNormal;
layout(location=2) out vec2 texcoord;
void main(){
 worldPosition=position;worldNormal=normal;
 bool planar=frame.options.x>.5||frame.camera.w>.5;
 texcoord=planar?vec2(position.x*.5+.5,.5-position.y*.5):uv;
 vec2 square=position.xy*vec2(min(1.,1./frame.options.w),min(1.,frame.options.w));
 gl_Position=planar?vec4(square.x,-square.y,0,1):frame.vp*vec4(position,1);
}
