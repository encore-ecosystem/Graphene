#!/usr/bin/env python3
"""Stage the actual local dependency closure for VG, without editor/UI modules."""
from pathlib import Path
from fixture_support import prepare_runtime_build
import re,shutil,subprocess,sys
repo=Path(__file__).resolve().parents[3];stage=repo/'target/animation/vg';(stage/'src').mkdir(parents=True,exist_ok=True)
seen=set()
def copy_module(path):
    path=path.resolve()
    if path in seen:return
    seen.add(path);source=path.read_text();dest=stage/'src'/path.relative_to(repo/'src');dest.parent.mkdir(parents=True,exist_ok=True)
    dest.write_text(source.replace('graphene::','refrain::'))
    for prefix,tail in re.findall(r'(?:pub\s+)?import\s+(mod|refrain|graphene)::([^\s\n{]+)',source):
        pieces=tail.rstrip(':').split('::')
        base=path.parent if prefix=='mod' else repo/'src'
        while pieces:
            stem=base.joinpath(*pieces)
            candidates=[stem.with_suffix('.enq'),stem/'mod.enq']
            found=next((p for p in candidates if p.is_file()),None)
            if found:copy_module(found);break
            pieces.pop()
for module in ['animation/source','animation/cook','animation/gpu','animation/gpu_section','virtual_geometry_gpu','rhi/mod','animation/runtime','scene/camera']:copy_module(repo/f'src/{module}.enq')
source=(repo/'tests/animation/virtual_geometry.enq').read_text().replace('graphene::','mod::').replace('FIXTURE_ROOT',str(repo/'target/animation'))
(stage/'src/main.enq').write_text(source)
fox=(repo/'tests/animation/fox_render.enq').read_text().replace('graphene::','refrain::').replace('FIXTURE_ROOT',str(repo/'target/animation'))
(stage/'src/fox_render.enq').write_text(fox)
(stage/'encore.toml').write_text('[project]\nname="graphene-animation-vg-check"\nversion="0.0.0"\nbuild="build.enq"\ndependencies=["index@std@0.0.5","index@json@0.0.3","index@binary@0.1.0","index@linalg@0.0.1","path@../../../../Vulkan","path@../../../../GPU","path@../../../../SDL"]\n')
prepare_runtime_build(repo, stage)
chunks=(repo/'encore.lock').read_text().split('[[packages]]');keep=[]
for chunk in chunks[1:]:
    if any(f'name = "{name}"' in chunk for name in ['std','json','binary','serialize','linalg','vulkan','gpu','sdl','core','platform']):
        for name in ['Vulkan','GPU','SDL']:chunk=chunk.replace('path@../'+name,'path@../../../../'+name)
        keep.append(chunk)
(stage/'encore.lock').write_text(chunks[0]+''.join('[[packages]]'+chunk for chunk in keep))
arguments=[a for a in sys.argv[1:] if a!='--release']
compiler=Path(arguments[0]) if arguments else repo.parent/'Encore/encore/target/release/encore'
profile='release' if '--release' in sys.argv else 'dev'
for args in [['check'],['build','--profile',profile,'--jobs','2']]:subprocess.run([str(compiler),*args],cwd=stage,check=True)
subprocess.run([str(stage/f'target/{profile}/graphene-animation-vg-check')],cwd=repo,check=True)
