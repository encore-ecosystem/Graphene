#define _POSIX_C_SOURCE 200809L
#include "../../../Vulkan/vulkan_api.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void *encore_str_from_cstr(const char *s){size_t n=strlen(s);encore_str_object *o=malloc(sizeof(*o)+n+1);assert(o);o->ref_count=1;o->len=n;memcpy(o->data,s,n+1);return o;}
static size_t device,target;
static size_t shader(const char *path){FILE *f=fopen(path,"rb");assert(f);fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);uint32_t *words=malloc(n);assert(fread(words,1,n,f)==(size_t)n);fclose(f);size_t h=encore_vk_shader_create(device,words,n/4,32);free(words);assert(encore_vk_shader_available(h));return h;}
static size_t create_buffer(const void *data,size_t bytes){size_t h=encore_vk_buffer_create(device,bytes,35,true);assert(encore_vk_buffer_available(h));if(data){void *p=encore_vk_buffer_map(h,bytes);assert(p);memcpy(p,data,bytes);encore_vk_buffer_unmap(h);}return h;}
static void dispatch(const char *shader_path,size_t *buffers,uint64_t *sizes,unsigned count,unsigned groups){
    uint32_t b[10],types[10],stages[10];uint64_t offsets[10]={0};for(unsigned i=0;i<count;i++){b[i]=i;types[i]=7;stages[i]=32;}
    size_t layout=encore_vk_bind_group_layout_create_entries(device,b,count,types,count,stages,count);assert(encore_vk_bind_group_layout_available(layout));
    size_t s=shader(shader_path),pipeline=encore_vk_compute_pipeline_create(device,s,layout);assert(encore_vk_compute_pipeline_available(pipeline));
    size_t group=encore_vk_buffer_bind_group_create_many(device,layout,b,count,buffers,count,offsets,count,sizes,count);assert(encore_vk_bind_group_available(group));
    size_t frame=encore_vk_frame_acquire(target);assert(encore_vk_frame_available(frame));
    assert(encore_vk_frame_dispatch_compute(frame,pipeline,group,groups,1,1));assert(encore_vk_frame_compute_barrier(frame));assert(encore_vk_frame_present(frame));assert(encore_vk_frame_destroy(frame));assert(encore_vk_swapchain_wait(target));
    encore_vk_bind_group_destroy(group);encore_vk_compute_pipeline_destroy(pipeline);encore_vk_shader_destroy(s);encore_vk_bind_group_layout_destroy(layout);
}
static void near(float a,float b){if(fabsf(a-b)>.0001f){fprintf(stderr,"Expected %.9g, got %.9g\n",b,a);abort();}}
static void test_skin(void){
    enum{N=129};uint32_t info[8]={N+1,1,2,1,1,N+1,1,2};
    struct Vertex{float position[4],normal[4],tangent[4],uv[4];uint32_t ja[4],jb[4];float wa[4],wb[4];}v={.position={1,2,3,1},.normal={1,1,1,0},.tangent={1,0,0,1},.ja={0,1,0,0},.wa={.25f,.75f,0,0}};
    _Static_assert(sizeof(v)==128,"Skin vertex ABI");
    float matrices[24]={2,0,0,2,0,1,0,0,0,0,.5f,0, 0,-3,0,-1,4,0,0,1,0,0,2,0};
    float previous[24]={1,0,0,0,0,1,0,0,0,0,1,0, 1,0,0,-1,0,1,0,0,0,0,1,0};
    uint32_t span[2]={0,1};struct Delta{float p[4],n[4],t[4];uint32_t target[4];}delta={.p={0,0,2,0},.n={0,1,0,0}};float weights[2]={.5f,.25f};
    uint32_t requests[N+1][8];for(unsigned i=0;i<N+1;i++){uint32_t r[8]={i==N?1:0,0,0,i,0,i%2,0,0};memcpy(requests[i],r,sizeof r);}
    float sentinel[(N+1)*20];for(unsigned i=0;i<(N+1)*20;i++)sentinel[i]=321;
    uint32_t status=0;const void *data[10]={info,&v,matrices,previous,span,&delta,weights,requests,sentinel,&status};uint64_t sizes[10]={sizeof info,sizeof v,sizeof matrices,sizeof previous,sizeof span,sizeof delta,sizeof weights,sizeof requests,sizeof sentinel,sizeof status};size_t buffers[10];for(unsigned i=0;i<10;i++)buffers[i]=create_buffer(data[i],sizes[i]);
    dispatch("target/animation/shaders/deform.spv",buffers,sizes,10,(N+1+63)/64);
    float result[(N+1)*20];assert(encore_vk_buffer_read_f32s(buffers[8],0,result,(N+1)*20));
    // Inverse transpose of the blended matrix, NOT a blend of transformed normals.
    float determinant=.5f*.25f+2.25f*3,normal[3]={(.25f-3*1.5f)/determinant,(2.25f+.5f*1.5f)/determinant,1/1.625f};float length=sqrtf(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);
    for(unsigned i=0;i<N;i++){float *r=result+i*20;near(r[0],-4.25f);near(r[1],4.25f);near(r[2],6.5f);for(unsigned c=0;c<3;c++)near(r[4+c],normal[c]/length);if(i%2){near(r[12],.25f);near(r[13],2);near(r[14],3.5f);near(r[15],1);}else{for(unsigned c=0;c<3;c++)near(r[12+c],r[c]);near(r[15],0);}}
    float old_normal[3]={1,1.25f,1},old_length=sqrtf(3.5625f);
    for(unsigned i=0;i<N;i++){float *r=result+i*20;for(unsigned c=0;c<3;c++)near(r[16+c],i%2?old_normal[c]/old_length:r[4+c]);near(r[19],0);}
    for(unsigned c=0;c<20;c++)near(result[N*20+c],321);
    assert(encore_vk_buffer_read_u32(buffers[9],0)==1);
    for(unsigned i=0;i<10;i++)encore_vk_buffer_destroy(buffers[i]);puts("GPU skin/morph: 129 requests, non-uniform normals, previous morph pose, invalid request: PASS");
}
static void test_bounds(void){
    uint32_t info[4]={1,2,2,1};struct Bound{float lo[4],hi[4];uint32_t bone[4];}bounds[2]={{{-1,-2,-3,0},{1,2,3,0},{0,0,0,0}},{{-1,-2,-3,0},{1,2,3,0},{1,0,0,0}}};
    float matrices[24]={2,0,0,2,0,1,0,0,0,0,.5f,0, 0,-3,0,-1,4,0,0,1,0,0,2,0};uint32_t request[4]={0,2,0,0},status=0;float output[12]={0};const void *data[6]={info,bounds,matrices,request,output,&status};uint64_t sizes[6]={sizeof info,sizeof bounds,sizeof matrices,sizeof request,sizeof output,sizeof status};size_t buffers[6];for(unsigned i=0;i<6;i++)buffers[i]=create_buffer(data[i],sizes[i]);dispatch("target/animation/shaders/bounds.spv",buffers,sizes,6,1);
    assert(encore_vk_buffer_read_f32s(buffers[4],0,output,12));near(output[4],-7);near(output[5],-3);near(output[6],-6);near(output[8],5);near(output[9],5);near(output[10],6);near(output[0],-1);near(output[1],1);near(output[2],0);near(output[3],sqrtf(88));assert(encore_vk_buffer_read_u32(buffers[5],0)==0);for(unsigned i=0;i<6;i++)encore_vk_buffer_destroy(buffers[i]);puts("GPU deformable bounds: conservative affine bone union: PASS");
}
int main(void){size_t instance=encore_vk_create(false);assert(encore_vk_available(instance));device=encore_vk_device_create(instance,0);assert(encore_vk_device_available(device));target=encore_vk_offscreen_create(device,8,8);assert(encore_vk_swapchain_available(target));test_skin();test_bounds();encore_vk_swapchain_destroy(target);encore_vk_device_destroy(device);encore_vk_destroy(instance);return 0;}
