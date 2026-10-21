#!/usr/bin/env python3
from pathlib import Path
from fixture_support import prepare_runtime_build
import shutil,subprocess,sys
repo=Path(__file__).resolve().parents[3];stage=repo/'target/animation/cache'
(stage/'src/animation').mkdir(parents=True,exist_ok=True)
for name in ['math','model','pose','sample','source','gpu_pack','gpu']:
    shutil.copyfile(repo/f'src/animation/{name}.enq',stage/f'src/animation/{name}.enq')
(stage/'src/rhi').mkdir(exist_ok=True)
shutil.copyfile(repo/'src/rhi/mod.enq',stage/'src/rhi/mod.enq')
source=(repo/'tests/animation/cache.enq').read_text().replace('graphene::animation','mod::animation').replace('graphene::rhi','mod::rhi').replace('FIXTURE_ROOT',str(repo/'target/animation'))
(stage/'src/main.enq').write_text(source)
(stage/'encore.toml').write_text('[project]\nname="graphene-animation-cache-check"\nversion="0.0.0"\nbuild="build.enq"\ndependencies=["index@std@0.0.5","index@linalg@0.0.1","path@../../../../Vulkan","path@../../../../GPU","path@../../../../SDL"]\n')
prepare_runtime_build(repo, stage)
chunks=(repo/'encore.lock').read_text().split('[[packages]]');keep=[]
for chunk in chunks[1:]:
    if any(f'name = "{name}"' in chunk for name in ['std','linalg','vulkan','gpu','sdl','core','platform']):
        for name in ['Vulkan','GPU','SDL']:chunk=chunk.replace('path@../'+name,'path@../../../../'+name)
        keep.append(chunk)
(stage/'encore.lock').write_text(chunks[0]+''.join('[[packages]]'+chunk for chunk in keep))
compiler=Path(sys.argv[1]) if len(sys.argv)>1 else repo.parent/'Encore/encore/target/release/encore'
for args in [['check'],['build']]:subprocess.run([str(compiler),*args],cwd=stage,check=True)
subprocess.run([str(stage/'target/dev/graphene-animation-cache-check')],cwd=repo,check=True)
