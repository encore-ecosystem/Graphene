#!/usr/bin/env python3
"""Exercise the actual per-view AA integration without compiling editor/UI policy."""
from pathlib import Path
import argparse,shutil,subprocess,struct,os
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--compiler',default='encore');p.add_argument('--direct',action='store_true');p.add_argument('--runtime',type=Path,default=root/'target/reconstruction/runtime/libencore_reconstruction.so');args=p.parse_args()
work=root/'target/reconstruction/view-fixture';src=work/'src';src.mkdir(parents=True,exist_ok=True)
files=['anti_aliasing.enq','temporal_view.enq','performance.enq','texture_gpu.enq','texture_image.enq','render_settings.enq','rhi/mod.enq','scene/camera.enq','scene/renderable.enq','scene/json_value.enq']
files += [str(f.relative_to(root/'src')) for f in (root/'src/shaders').glob('aa_*.enq')]
for name in files:
 destination=src/name;destination.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/'src'/name,destination)
shutil.copy2(root/'build.enq',work/'build.enq');shutil.copy2(root/'encore.lock',work/'encore.lock')
(work/'encore.toml').write_text('[project]\nname="reconstruction-view-fixture"\nversion="0.0.0"\nbuild="build.enq"\ndependencies=["index@std@0.0.5","index@json@0.0.3","index@linalg@0.0.1",'+','.join('"path@'+str(root.parent/n)+'"' for n in ['Vulkan','SDL','GPU'])+']\n')
# Absolute paths appear only in this generated local fixture, never package manifests.
c='#define _GNU_SOURCE\n#include <time.h>\n#include <stdio.h>\n#include <stdint.h>\n#include <stdlib.h>\n#include <math.h>\n#include "'+str(root.parent/'Vulkan/vulkan_api.h')+'"\n'
for f in ['native/aa_shader_data.c','native/smaa_tables.c','native/json_number.c']:c+='#include "'+str(root/f)+'"\n'
c+='uint64_t graphene_perf_counter_ns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;}\n'
for stage,source in [('vert','#version 450\nvoid main(){vec2 p=vec2((gl_VertexIndex<<1)&2,gl_VertexIndex&2);gl_Position=vec4(p*2-1,.5,1);}\n'),('frag','#version 450\nlayout(location=0)out vec4 color;layout(location=1)out vec4 motion;void main(){color=vec4(.25,.5,.75,1);motion=vec4(0,0,1,1);}\n')]:
 path=work/('draw.'+stage);path.write_text(source);binary=path.with_suffix('.'+stage+'.spv')
 subprocess.run(['glslc','--target-env=vulkan1.2',str(path),'-o',str(binary)],check=True)
 data=binary.read_bytes();words=struct.unpack('<'+'I'*(len(data)//4),data)
 c+='static const uint32_t fixture_'+stage+'[]={'+','.join(str(x) for x in words)+'};\n'
c+='uint32_t fixture_shader(uint32_t stage,uint32_t index){const uint32_t *p=stage?fixture_frag:fixture_vert;return p[index];}\n'
c+='uint32_t fixture_shader_size(uint32_t stage){return stage?sizeof(fixture_frag)/4:sizeof(fixture_vert)/4;}\n'
c+='''bool fixture_pixels(size_t target){
 size_t n=encore_vk_target_readback(target,0,0,0);uint8_t *p=malloc(n);if(!p)return false;
 bool ok=n && encore_vk_target_readback(target,0,(uintptr_t)p,n)==n;
 if(ok){size_t center=(n/8)/4*4;ok=abs((int)p[center]-225)<18 && abs((int)p[center+1]-188)<18 && abs((int)p[center+2]-137)<18;
 if(!ok)fprintf(stderr,"Unexpected BGRA output: %u %u %u %u\\n",p[center],p[center+1],p[center+2],p[center+3]);}
 free(p);return ok;
}
'''
(work/'runtime.c').write_text(c)
(src/'main.enq').write_text('''import core::vec::Vec
import core::option::Option
import linalg::Vec3
import mod::rhi::*
import mod::anti_aliasing::AntiAliasingView
import mod::render_settings::*
import mod::scene::camera::PerspectiveCamera
import mod::scene::renderable::RenderItem
import std::io::println
extern fn fixture_shader(stage:u32,index:u32)->u32
extern fn fixture_shader_size(stage:u32)->u32
extern fn fixture_pixels(target:usize)->bool
fn shader(stage:u32)->Vec[u32]{let mut words=Vec[u32]::new() unsafe{for i in 0..(fixture_shader_size(stage) as usize){words.push(fixture_shader(stage,i as u32))}} ret words}
fn main()->u32 {
 let instance=GraphicsInstance::create()
 let adapter=match instance.preferred_adapter(){Some(value)=>value None=>{ret 1}}
 let device=instance.create_device(adapter)
 let composition=device.create_render_target(RenderTargetDescriptor::new(1280,720,format_bgra8_unorm()))
 let mut entries=vec![BindGroupLayoutEntry::uniform(0,shader_stage_vertex()|shader_stage_fragment()),BindGroupLayoutEntry::storage(1,shader_stage_vertex())]
 for i in 2..5 {entries.push(BindGroupLayoutEntry::storage(i as u32,shader_stage_fragment()))}
 for i in 5..8 {entries.push(BindGroupLayoutEntry::texture_sampler(i as u32,shader_stage_fragment()))}
 let layout=device.create_bind_group_layout(entries)
 let empty=layout
 let mut aa=AntiAliasingView::create(device,composition,layout)
 let vertex=device.create_shader_module(shader(0),shader_stage_vertex()) let fragment=device.create_shader_module(shader(1),shader_stage_fragment())
 let mut vertices=Vec[f32]::new() for i in 0..33 {vertices.push(0_f32)}
 let mesh=device.create_mesh(vertices,vec![0_u32,1_u32,2_u32])
 let camera=PerspectiveCamera::new(Vec3::new(0,0,3),Vec3::zero(),1.047_f32,0.1_f32,10000)
 for method in anti_aliasing_methods(){
  let maximum=if method.key()=="taa" || method.key()=="fsr" || method.key()=="dlss" {2_u32}else{1_u32}
  for scenario in 0..(maximum as usize) {
   let settings=RenderSettings::defaults().with_anti_aliasing(method).with_quality(UpscalingQuality::Performance).with_frame_generation(if scenario==0 {FrameGenerationMethod::Off} elif method.key()=="dlss" {FrameGenerationMethod::Dlss}else{FrameGenerationMethod::Fsr}).with_multiplier(if scenario>0 && method.key()=="dlss" {4_u32}else{2_u32})
   aa.set_settings(settings) aa.set_playing(scenario>0)
   if !aa.configure(method,1280,720,composition){println("configure failed "+method.key()) ret 2}
   let pipeline=device.create_render_pipeline_with_options(aa.target(),vertex,fragment,VertexBufferLayout::empty(),empty,RenderPipelineOptions::depth())
   if !pipeline.available(){println("fixture pipeline failed: "+pipeline.error()) ret 3}
   for n in 0..4 {
    aa.observe_camera(camera)
    let raw=aa.target().acquire_frame() let pass=raw.begin_render_pass(pipeline,0,0,0,0)
    let rendered=pass.draw_mesh(mesh) && pass.end() && raw.present() raw.destroy()
    if !rendered {println("source failed") ret 4}
    let frame=composition.acquire_frame()
    let composed=aa.composite(frame,0,0) && aa.end_composite(frame) && frame.present() frame.destroy()
    if !composed {println("composite failed: "+method.key()+" "+aa.reconstruction_error()) ret 5}
    if !composition.wait_readback(){ret 6}
    if !unsafe{fixture_pixels(composition.handle())}{println("pixels failed: "+method.key()) ret 7}
    aa.commit(Vec[RenderItem]::new(),aa.camera(camera)) aa.begin_presentations(16000000_u64,1_u64)
    while aa.presentation_pending(){aa.presentation_submitted(1_u64)}
    if aa.reconstruction_error()!=""{println(aa.reconstruction_error()) ret 8}
   }
   println("View integration: requested "+method.name()+", active "+aa.method().name()+", FG preference "+settings.frame_generation().key()+" PASS")
   composition.wait_readback() pipeline.destroy()
  }
 }
 aa.destroy() mesh.destroy() vertex.destroy() fragment.destroy() layout.destroy() composition.destroy() device.destroy() instance.destroy()
 ret 0
}
''')
if args.direct:
 program=(src/'main.enq').read_text().replace('let mut aa=AntiAliasingView::create(device,composition,layout)','let mut aa=AntiAliasingView::create(device,composition,layout) aa.set_direct_present(true)')
 program=program.replace('while aa.presentation_pending(){aa.presentation_submitted(1_u64)}', '''aa.presentation_submitted(1_u64)
    while aa.presentation_pending(){
     let next=composition.acquire_frame()
     let presented=aa.composite_presentation(next) && next.present() next.destroy()
     if !presented || !composition.wait_readback(){ret 9}
     if !unsafe{fixture_pixels(composition.handle())}{ret 10}
     aa.presentation_submitted(1_u64)
    }''')
 (src/'main.enq').write_text(program)
env=dict(os.environ,ENCORE_RECONSTRUCTION_LIBRARY=str(args.runtime.resolve()))
subprocess.run([args.compiler,'run'],cwd=work,env=env,check=True)
