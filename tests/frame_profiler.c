#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
static uint64_t now, ready_id, wait_ns;
uint64_t graphene_perf_counter_ns(void) { return now; }
uint32_t encore_vk_timing_support(size_t d){return d!=0;}
bool encore_vk_timing_begin(size_t d,uint64_t id){(void)id;return d!=0;}
void encore_vk_timing_finish(size_t d,bool valid){(void)d;(void)valid;}
uint64_t encore_vk_timing_wait_ns(size_t d){(void)d;return wait_ns;}
uint64_t encore_vk_timing_poll(size_t d){(void)d;return ready_id;}
uint64_t encore_vk_timing_value(size_t d,uint32_t e,uint32_t f){(void)d;(void)e;return f==0?1:f==1?1:f==4?2000000:0;}
void encore_vk_timing_release(size_t d){(void)d;ready_id=0;}
#include "../native/profiling/frame_profiler.inc"
static void capture(uint64_t duration,bool valid){wait_ns=0;graphene_profiler_begin();now+=duration;graphene_profiler_finish(valid);}
int main(void){
 graphene_profiler_enable(1,true);capture(10000000,true);
 assert(graphene_profiler_value(0,0)==1 && graphene_profiler_value(0,7)==0);
 ready_id=1;capture(20000000,true);
 assert(graphene_profiler_value(UINT32_MAX,2)==1 && graphene_profiler_value(UINT32_MAX,5)==2000000);
 assert(graphene_profiler_stat(0)==15000000);
 graphene_profiler_select(0);uint64_t pinned=graphene_profiler_value(UINT32_MAX,2);
 capture(10000000,false);assert(graphene_profiler_value(0,0)==2);
 for(unsigned n=0;n<650;++n)capture(10000000,true);
 assert(graphene_profiler_value(0,0)==600 && graphene_profiler_value(UINT32_MAX,2)==pinned);
 graphene_profiler_live();assert(graphene_profiler_value(0,1)==1);
 graphene_profiler_clear();assert(!graphene_profiler_value(0,0));
 for(unsigned n=0;n<60;++n){capture(12000000,true);GrapheneFrameProfile*f=graphene_profile_at(n);f->gpu_state=1;f->gpu_ns=2000000;}
 assert(graphene_profiler_bottleneck()==1);
 for(unsigned n=0;n<60;++n){GrapheneFrameProfile*f=graphene_profile_at(n);f->gpu_ns=20000000;}
 assert(graphene_profiler_bottleneck()==2);
 for(unsigned n=0;n<60;++n){GrapheneFrameProfile*f=graphene_profile_at(n);f->wait_ns=30000000;}
 assert(graphene_profiler_bottleneck()==3);
 graphene_profiler_clear();capture(10000000,false);graphene_profiler_event_wait(3000000);capture(5000000,true);
 assert(graphene_profiler_value(0,4)==15000000 && graphene_profiler_value(0,6)==3000000);
 assert(graphene_profiler_value(0,8)==2); /* successful work plus deferred work */
 graphene_profiler_ui(10,20,30,40);graphene_profiler_begin();graphene_profiler_ui(20,40,70,140);now+=1000;graphene_profiler_finish(true);
 assert(graphene_profiler_value(1,12)==10 && graphene_profiler_value(1,14)==40 && graphene_profiler_value(1,15)==100);
 graphene_profiler_enable(1,false);unsigned count=graphene_profiler.count;capture(50000000,true);assert(count==graphene_profiler.count);
 puts("frame history, asynchronous pairing and bottleneck analysis: pass");return 0;
}

void encore_vk_parallel_frame_begin(size_t device,uint64_t id){(void)device;(void)id;}
uint64_t encore_vk_parallel_frame_value(size_t device,uint64_t id,uint32_t thread,uint32_t field){(void)device;(void)id;(void)thread;(void)field;return 0;}
