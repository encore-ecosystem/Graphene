/* GPU-generated post-cull dispatch: no candidates, one and multiple groups.
 * The same test covers synchronous and parallel native command recording. */
#include "../../Vulkan/backend.c"
#include <assert.h>
void *encore_str_from_cstr(const char *s){return (void *)s;}
static size_t load_shader(size_t device,const char *path) {
    FILE *f=fopen(path,"rb");assert(f);fseek(f,0,SEEK_END);size_t bytes=(size_t)ftell(f);rewind(f);
    uint32_t *words=malloc(bytes);assert(fread(words,1,bytes,f)==bytes);fclose(f);
    size_t result=encore_vk_shader_create(device,words,bytes/4,32);free(words);assert(encore_vk_shader_available(result));return result;
}
int main(int argc,char **argv) {
    assert(argc==4);assert(encore_vk_parallel_configure((uint32_t)atoi(argv[3])));
    size_t instance=encore_vk_create(false),device=encore_vk_device_create(instance,0);assert(encore_vk_device_available(device));
    const uint32_t clusters=256,base=64+256*16+28+256*4+256+6134;
    size_t buffer=encore_vk_buffer_create(device,(base+10)*4,32|256|2,true);assert(encore_vk_buffer_available(buffer));
    uint32_t binding=0,type=7,stage=32;
    size_t layout=encore_vk_bind_group_layout_create_entries(device,&binding,1,&type,1,&stage,1);
    size_t group=encore_vk_storage_bind_group_create(device,layout,buffer,0,(base+10)*4);assert(encore_vk_bind_group_available(group));
    size_t args_shader=load_shader(device,argv[1]),probe_shader=load_shader(device,argv[2]);
    size_t args=encore_vk_compute_pipeline_create(device,args_shader,layout),probe=encore_vk_compute_pipeline_create(device,probe_shader,layout);
    assert(encore_vk_compute_pipeline_available(args)&&encore_vk_compute_pipeline_available(probe));
    size_t target=encore_vk_offscreen_create(device,8,8);
    for(int i=0;i<2;++i)assert(encore_vk_swapchain_clear(target,0,0,0,1));assert(encore_vk_swapchain_wait(target));
    uint32_t counts[]={0,1,65,1000};
    for(unsigned n=0;n<4;++n) {
        uint32_t count=counts[n],bounded=count>clusters?clusters:count;
        assert(encore_vk_buffer_write_u32(buffer,0,clusters));assert(encore_vk_buffer_write_u32(buffer,8,0));
        assert(encore_vk_buffer_write_f32(buffer,39*4,1));assert(encore_vk_buffer_write_u32(buffer,(base+3)*4,count));
        assert(encore_vk_buffer_write_u32(buffer,60*4,0));
        size_t frame=encore_vk_frame_acquire(target);assert(encore_vk_frame_available(frame));
        assert(encore_vk_frame_dispatch_compute(frame,args,group,1,1,1));assert(encore_vk_frame_compute_barrier(frame));
        assert(encore_vk_frame_dispatch_compute_indirect(frame,probe,group,buffer,(base+7)*4));assert(encore_vk_frame_compute_barrier(frame));
        assert(!encore_vk_frame_dispatch_compute_indirect(frame,probe,group,buffer,3));
        assert(!encore_vk_frame_dispatch_compute_indirect(frame,probe,group,buffer,(base+10)*4));
        assert(encore_vk_frame_present(frame));encore_vk_frame_destroy(frame);assert(encore_vk_swapchain_wait(target));
        assert(encore_vk_buffer_read_u32(buffer,(base+7)*4)==(bounded+63)/64);
        assert(encore_vk_buffer_read_u32(buffer,60*4)==((bounded+63)/64)*64);
    }
    encore_vk_swapchain_destroy(target);encore_vk_compute_pipeline_destroy(args);encore_vk_compute_pipeline_destroy(probe);
    encore_vk_shader_destroy(args_shader);encore_vk_shader_destroy(probe_shader);encore_vk_bind_group_destroy(group);
    encore_vk_bind_group_layout_destroy(layout);encore_vk_buffer_destroy(buffer);encore_vk_device_destroy(device);encore_vk_destroy(instance);
    puts("post-culling indirect dispatch: empty, one, multiple and capacity bound passed");
}
