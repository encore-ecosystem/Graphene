#!/usr/bin/env python3
"""Stage the real bridge, not a mocked importer, with a small Encore package."""
from pathlib import Path
from fixture_support import prepare_runtime_build
import subprocess, shutil, sys
repo=Path(__file__).resolve().parents[3];stage=repo/'target/animation/source'
(stage/'src/animation').mkdir(parents=True,exist_ok=True)
for name in ['math','model','pose','sample','deform','material','source']:
    shutil.copyfile(repo/f'src/animation/{name}.enq',stage/f'src/animation/{name}.enq')
source=(repo/'tests/animation/source.enq').read_text().replace('graphene::animation','mod::animation').replace('FIXTURE_ROOT',str(repo/'target/animation'))
(stage/'src/main.enq').write_text(source)
(stage/'encore.toml').write_text('[project]\nname="graphene-animation-source-check"\nversion="0.0.0"\nbuild="build.enq"\ndependencies=["index@std@0.0.5","path@../../../../Vulkan"]\n')
prepare_runtime_build(repo, stage)
compiler=Path(sys.argv[1]) if len(sys.argv)>1 else repo.parent/'Encore/encore/target/release/encore'
for args in [['check'],['build']]:subprocess.run([str(compiler),*args],cwd=stage,check=True)
subprocess.run([str(stage/'target/dev/graphene-animation-source-check')],cwd=repo,check=True)
print('Animation source integration: PASS')
