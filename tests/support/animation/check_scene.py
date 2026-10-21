#!/usr/bin/env python3
"""Exercise actual scene binding and background cooked-asset loading."""
from pathlib import Path
from fixture_support import prepare_runtime_build
import re,shutil,subprocess,sys
repo=Path(__file__).resolve().parents[3];stage=repo/'target/animation/scene';(stage/'src').mkdir(parents=True,exist_ok=True)
seen=set()
def copy_module(path):
    path=path.resolve()
    if path in seen:return
    seen.add(path);source=path.read_text();dest=stage/'src'/path.relative_to(repo/'src');dest.parent.mkdir(parents=True,exist_ok=True);dest.write_text(source.replace("graphene::","refrain::"))
    for prefix,tail in re.findall(r'(?:pub\s+)?import\s+(mod|refrain|graphene)::([^\s\n{]+)',source):
        pieces=tail.rstrip(':').split('::');base=path.parent if prefix=='mod' else repo/'src'
        while pieces:
            stem=base.joinpath(*pieces);found=next((p for p in [stem.with_suffix('.enq'),stem/'mod.enq'] if p.is_file()),None)
            if found:copy_module(found);break
            pieces.pop()
for module in ['animation/scene','animation/component','animation/picking','scene/asset']:copy_module(repo/f'src/{module}.enq')
(stage/'src/main.enq').write_text((repo/'tests/animation/scene.enq').read_text().replace('graphene::','mod::').replace('FIXTURE_ROOT',str(repo/'target/animation')))
(stage/'encore.toml').write_text('[project]\nname="graphene-animation-scene-check"\nversion="0.0.0"\nbuild="build.enq"\ndependencies=["index@std@0.0.5","index@json@0.0.3","index@binary@0.1.0","index@linalg@0.0.1","path@../../../../Vulkan"]\n')
prepare_runtime_build(repo, stage)
chunks=(repo/'encore.lock').read_text().split('[[packages]]')
(stage/'encore.lock').write_text(chunks[0]+''.join('[[packages]]'+c.replace('path@../Vulkan','path@../../../../Vulkan') for c in chunks[1:] if any(f'name = "{n}"' in c for n in ['std','json','serialize','binary','linalg','vulkan','core','platform'])))
compiler=Path(sys.argv[1]) if len(sys.argv)>1 else repo.parent/'Encore/encore/target/release/encore'
for args in [['check'],['build']]:subprocess.run([str(compiler),*args],cwd=stage,check=True)
subprocess.run([str(stage/'target/dev/graphene-animation-scene-check')],cwd=repo,check=True)
print('Animation scene binding: PASS')
