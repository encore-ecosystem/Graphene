#ifndef GRAPHENE_VFX_COMMON
#define GRAPHENE_VFX_COMMON
layout(std140,binding=0) uniform Settings {
 mat4 vp;vec4 right,up,camera,origin,direction,color,forces,state,control,topology;vec4 parameters[64];
} u;
layout(std430,binding=1) buffer Particles {float data[];};
layout(std430,binding=2) buffer Pool {uint pool[];};
layout(std430,binding=3) readonly buffer Program {float program[];};
layout(std430,binding=4) buffer Commands {uint commands[];};
layout(std430,binding=5) buffer Events {uint eventCount;uint eventDropped;uint eventPad0,eventPad1;vec4 eventData[];};
uint gvfxScope=0;bool gvfxCollision=false;
uint capacity(){return uint(u.state.z);}
float getv(uint slot,uint component){return data[gvfxScope==0?component*capacity()+slot:capacity()*32+(gvfxScope-1)*32+component];}
void setv(uint slot,uint component,float v){data[gvfxScope==0?component*capacity()+slot:capacity()*32+(gvfxScope-1)*32+component]=(isnan(v)||isinf(v))?0:v;}
vec4 attr(uint slot,uint a){return vec4(getv(slot,a*4),getv(slot,a*4+1),getv(slot,a*4+2),getv(slot,a*4+3));}
void storev(uint slot,uint a,vec4 v){for(uint k=0;k<4;k++)setv(slot,a*4+k,v[k]);}
uint seedv(){return uint(u.state.w)|(uint(u.topology.x)<<16);}
uint hashv(uint x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return x;}
uint particleKey(uint slot){uint low=uint(getv(slot,19))|(uint(getv(slot,21))<<16),high=uint(getv(slot,22))|(uint(getv(slot,23))<<16);return low^hashv(high);}
float randomv(uint x){return float(hashv(x)>>8)*(1.0/16777216.0);}
void emitEvent(uint kind,uint slot,float extra){uint i=atomicAdd(eventCount,1);if(i>=4096){atomicAdd(eventDropped,1);return;}eventData[i*4]=vec4(kind,getv(slot,19),u.state.x,extra);eventData[i*4+1]=vec4(attr(slot,0).xyz,float(seedv()&65535u));eventData[i*4+2]=vec4(attr(slot,1).xyz,float(seedv()>>16));eventData[i*4+3]=vec4(getv(slot,20),getv(slot,21),getv(slot,22),getv(slot,23));}
void execute(uint slot,uint stage){
 vec4 r[32];for(uint j=0;j<32;j++)r[j]=vec4(0);
 for(uint ip=0;ip<uint(program[0]);ip++){
  uint p=16+ip*16;if(uint(program[p+10])!=stage)continue;
  uint op=uint(program[p]),dst=uint(program[p+1]),a=uint(program[p+2]),b=uint(program[p+3]),c=uint(program[p+8]),at=uint(program[p+9]);
  vec4 v=vec4(program[p+4],program[p+5],program[p+6],program[p+7]),outv=vec4(0),x=r[a],y=r[b];float len=length(x.xyz);
  switch(op){
   case 1:outv=v;break;case 2:outv=attr(slot,at);break;case 3:storev(slot,at,x);continue;
   case 4:outv=x+y;break;case 5:outv=x-y;break;case 6:outv=x*y;break;
   case 7:for(int k=0;k<4;k++)outv[k]=abs(y[k])>1e-12?x[k]/y[k]:0;break;
   case 8:outv=mix(x,y,r[c]);break;case 9:outv=clamp(x,vec4(v.x),vec4(v.y));break;
   case 10:outv=sin(x);break;case 11:outv=cos(x);break;case 12:outv=vec4(len>1e-12?x.xyz/len:vec3(0),0);break;
   case 13:outv=vec4(len);break;case 14:outv=vec4(dot(x.xyz,y.xyz));break;
   case 15:for(uint k=0;k<4;k++)outv[k]=randomv(seedv()^particleKey(slot)^(uint(v.x)*7919u)^(k*104729u));break;
   case 16:outv=sin(x*12.9898+y*78.233+v.x);break;
   case 17:outv=uint(v.x)==0?vec4(u.state.x):uint(v.x)==1?vec4(u.state.y):uint(v.x)==2?vec4(getv(slot,3)/max(.00001,getv(slot,7))):uint(v.x)==3?vec4(getv(slot,19),getv(slot,21),getv(slot,22),getv(slot,23)):uint(v.x)==4?u.origin:vec4(0);break;
   case 18:outv=u.parameters[uint(v.x)];break;
   case 19:{uint base=uint(v.x),n=uint(v.y),j=0;float t=x.x;while(j+1<n&&program[base+(j+1)*8]<=t)j++;uint next=min(j+1,n-1);float x0=program[base+j*8],x1=program[base+next*8];float f=x1>x0?clamp((t-x0)/(x1-x0),0,1):0;for(uint k=0;k<4;k++)outv[k]=mix(program[base+j*8+4+k],program[base+next*8+4+k],f);break;}
   case 20:outv=r[c].x>.5?x:y;break;case 21:outv=vec4(lessThan(x,y));break;case 22:outv=vec4(greaterThan(x,y));break;case 23:outv=vec4(cross(x.xyz,y.xyz),0);break;
   case 24:case 25:case 26:{uint seed=seedv()^particleKey(slot)^(ip*0x9e3779b9u);vec3 q=vec3(randomv(seed),randomv(seed+1),randomv(seed+2))*2-1;
    if(op==24){float l=length(q);q=l>.00001?q/l*v.x*pow(randomv(seed+3),1.0/3.0):vec3(0);}if(op==25)q*=v.xyz;
    if(op==26){float theta=randomv(seed)*6.28318530718,cp=1-randomv(seed+1)*(1-cos(v.x)),sp=sqrt(max(0,1-cp*cp));q=vec3(cos(theta)*sp,cp,sin(theta)*sp)*v.y;}outv=vec4(q,0);break;}
   case 27:if(getv(slot,1)<v.x){setv(slot,1,v.x);setv(slot,5,abs(getv(slot,5))*clamp(v.y,0,1));emitEvent(3,slot,v.y);gvfxCollision=true;}continue;
   case 28:{vec3 d=x.xyz-attr(slot,0).xyz;float dist=length(d);if(dist>.00001)for(uint k=0;k<3;k++)setv(slot,4+k,getv(slot,4+k)+d[k]/dist*v.x*u.state.y);continue;}
   case 29:if(x.x>.5)setv(slot,3,getv(slot,7));continue;
   case 30:if(x.x>.5)emitEvent(4,slot,v.x);continue;
   case 31:outv=abs(x);break;case 32:outv=min(x,y);break;case 33:outv=max(x,y);break;case 34:outv=pow(max(x,vec4(0)),y);break;case 35:outv=floor(x);break;case 36:outv=fract(x);break;
  }
  for(uint k=0;k<4;k++)r[dst][k]=(isnan(outv[k])||isinf(outv[k]))?0:outv[k];
 }
}
#endif
