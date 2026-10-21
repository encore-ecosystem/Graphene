#version 450
layout(location=0) in vec3 position;
layout(location=0) out float height;
layout(set=0,binding=0) uniform Frame {mat4 vp;vec4 lightDirection;vec4 energy;vec4 ambient;vec4 camera;} frame;
struct Instance {mat4 model;vec4 color;vec4 surface;};
layout(std430,set=0,binding=1) readonly buffer Instances {Instance values[];} instances;
void main(){vec3 world=(instances.values[gl_InstanceIndex].model*vec4(position,1.)).xyz;height=world.y;gl_Position=vec4((world.x-frame.camera.x)/128.,(world.z-frame.camera.z)/128.,(frame.camera.y-world.y)/256.,1.);}
