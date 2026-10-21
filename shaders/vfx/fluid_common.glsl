#ifndef GRAPHENE_VFX_FLUID
#define GRAPHENE_VFX_FLUID
layout(std140,binding=0) uniform FluidSettings {mat4 vp;vec4 camera,forward,right,up,origin,extent,grid,step,source,sourceValues,sourceVelocity,settings,color,obstacle,obstacleVelocity,render,liquidConfig;} f;
layout(std430,binding=1) buffer FluidState {vec4 cells[];};
layout(std430,binding=2) buffer FluidScratch {vec4 temporary[];};
layout(std430,binding=3) buffer FluidPressure {float pressure[];};
layout(std430,binding=4) buffer FluidDivergence {float divergence[];};
layout(std430,binding=5) buffer LiquidParticles {uint liquidCount,maxSpeedBits,fluidDropped,previousMaxSpeedBits;vec4 particles[];};
layout(std430,binding=6) buffer LiquidBins {uint bins[];};
uvec3 dimensions(){return uvec3(f.grid.xyz);}
uint cellCount(){uvec3 d=dimensions();return d.x*d.y*d.z;}
uint cellIndex(ivec3 p){ivec3 d=ivec3(dimensions());p=clamp(p,ivec3(0),d-1);return (uint(p.z)*uint(d.y)+uint(p.y))*uint(d.x)+uint(p.x);}
ivec3 cellPosition(uint i){uvec3 d=dimensions();return ivec3(i%d.x,(i/d.x)%d.y,i/(d.x*d.y));}
bool insideCell(ivec3 p){return all(greaterThanEqual(p,ivec3(0)))&&all(lessThan(p,ivec3(dimensions())));}
bool solidCell(ivec3 p){if(!insideCell(p))return true;vec3 q=(vec3(p)+.5)*f.grid.w;return f.obstacle.w>0&&distance(q,f.obstacle.xyz)<f.obstacle.w;}
vec4 sampleState(vec3 p,uint component){p=clamp(p,vec3(0),f.grid.xyz-1);ivec3 i=ivec3(floor(p));vec3 t=fract(p);vec4 outv=vec4(0);for(int z=0;z<2;z++)for(int y=0;y<2;y++)for(int x=0;x<2;x++){vec3 w=mix(1-t,t,vec3(x,y,z));outv+=cells[cellIndex(i+ivec3(x,y,z))*2+component]*w.x*w.y*w.z;}return outv;}
vec3 sampleOld(vec3 p){p=clamp(p,vec3(0),f.grid.xyz-1);ivec3 i=ivec3(floor(p));vec3 t=fract(p);vec3 outv=vec3(0);for(int z=0;z<2;z++)for(int y=0;y<2;y++)for(int x=0;x<2;x++){vec3 w=mix(1-t,t,vec3(x,y,z));outv+=temporary[cellIndex(i+ivec3(x,y,z))*2].xyz*w.x*w.y*w.z;}return outv;}
float safeDelta(){float speed=uintBitsToFloat(previousMaxSpeedBits);return min(f.step.x,.35*f.grid.w/max(.0001,speed));}
uint fluidHash(uint x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return x;}
float fluidRandom(uint x){return float(fluidHash(x)>>8)/16777216.0;}
#endif
