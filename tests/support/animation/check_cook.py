#!/usr/bin/env python3
"""Real cook/codec modules, isolated from editor/UI compilation."""
from pathlib import Path
from fixture_support import prepare_runtime_build
import shutil,subprocess,sys
repo=Path(__file__).resolve().parents[3];stage=repo/'target/animation/cook'
(stage/'src/animation').mkdir(parents=True,exist_ok=True)
for name in ['math','model','pose','sample','deform','material','source','cook','asset','character']:
    shutil.copyfile(repo/f'src/animation/{name}.enq',stage/f'src/animation/{name}.enq')
for name in ['virtual_geometry','native_asset','standard_material','texture_image','scene/primitive','content_browser/path']:
    p=stage/f'src/{name}.enq';p.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(repo/f'src/{name}.enq',p)
source=(repo/'tests/animation/cook.enq').read_text().replace('graphene::animation','mod::animation').replace('FIXTURE_ROOT',str(repo/'target/animation'))
(stage/'src/main.enq').write_text(source)
static=(repo/'tests/virtual_geometry_uv_seams.enq').read_text().replace('graphene::virtual_geometry','refrain::virtual_geometry').replace('fn main()','pub fn check_static()')
(stage/'src/static_regression.enq').write_text(static)
(stage/'encore.toml').write_text('[project]\nname="graphene-animation-cook-check"\nversion="0.0.0"\nbuild="build.enq"\ndependencies=["index@std@0.0.5","index@binary@0.1.0","index@linalg@0.0.1","path@../../../../Vulkan"]\n')
prepare_runtime_build(repo, stage)
# Resolve transitive version ranges using the project's approved lock, so
# a new index release cannot silently change this focused regression fixture.
chunks=(repo/'encore.lock').read_text().split('[[packages]]')
keep=[]
for chunk in chunks[1:]:
    if any(f'name = "{name}"' in chunk for name in ['std','binary','serialize','linalg','vulkan','core','platform']):
        keep.append(chunk.replace('path@../Vulkan','path@../../../../Vulkan'))
(stage/'encore.lock').write_text(chunks[0]+''.join('[[packages]]'+chunk for chunk in keep))
compiler=Path(sys.argv[1]) if len(sys.argv)>1 else repo.parent/'Encore/encore/target/release/encore'
for args in [['check'],['build']]:subprocess.run([str(compiler),*args],cwd=stage,check=True)
subprocess.run([str(stage/'target/dev/graphene-animation-cook-check')],cwd=repo,check=True)
print('Animation cook and native codec: PASS')
