#!/usr/bin/env python3
"""Strict, source-independent skeletal scene policy codec."""
from pathlib import Path
from fixture_support import prepare_runtime_build
import shutil,subprocess,sys
repo=Path(__file__).resolve().parents[3];stage=repo/'target/animation/component'
for name in ['animation/math','animation/component','scene/json_value']:
    p=stage/f'src/{name}.enq';p.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(repo/f'src/{name}.enq',p)
(stage/'src/main.enq').write_text((repo/'tests/animation/component.enq').read_text().replace('graphene::','mod::'))
(stage/'encore.toml').write_text('[project]\nname="graphene-animation-component-check"\nversion="0.0.0"\nbuild="build.enq"\ndependencies=["index@std@0.0.5","index@json@0.0.3","index@linalg@0.0.1","path@../../../../Vulkan"]\n')
prepare_runtime_build(repo, stage)
chunks=(repo/'encore.lock').read_text().split('[[packages]]')
(stage/'encore.lock').write_text(chunks[0]+''.join('[[packages]]'+c.replace('path@../Vulkan','path@../../../../Vulkan') for c in chunks[1:] if any(f'name = "{n}"' in c for n in ['std','json','serialize','linalg','vulkan','core','platform'])))
compiler=Path(sys.argv[1]) if len(sys.argv)>1 else repo.parent/'Encore/encore/target/release/encore'
for args in [['check'],['build']]:subprocess.run([str(compiler),*args],cwd=stage,check=True)
subprocess.run([str(stage/'target/dev/graphene-animation-component-check')],cwd=repo,check=True)
print('Skeletal scene component: PASS')
