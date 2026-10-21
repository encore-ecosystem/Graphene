#!/usr/bin/env python3
"""Run the actual animation core in a dependency-free Encore fixture.
No editor/Vulkan rebuild is needed for deterministic runtime regressions.
"""
from pathlib import Path
import shutil, subprocess, sys
repo=Path(__file__).resolve().parents[3]
stage=repo/'target/animation/core'
(stage/'src/animation').mkdir(parents=True,exist_ok=True)
for name in ['math','model','pose','sample','blend','player','root_motion','deform','runtime','ik','graph','state_machine','motion_matching','blend_space','controller']:
    shutil.copyfile(repo/f'src/animation/{name}.enq',stage/f'src/animation/{name}.enq')
(stage/'src/main.enq').write_text((repo/'tests/animation/core.enq').read_text().replace('graphene::animation','mod::animation'))
(stage/'encore.toml').write_text('[project]\nname="graphene-animation-core-check"\nversion="0.0.0"\n')
compiler=Path(sys.argv[1]) if len(sys.argv)>1 else repo.parent/'Encore/encore/target/release/encore'
for args in [['check'],['build']]:
    subprocess.run([str(compiler),*args],cwd=stage,check=True)
subprocess.run([str(stage/'target/dev/graphene-animation-core-check')],check=True)
print('Animation core: PASS')
