#!/usr/bin/env python3
"""Benchmark the production CPU animation World: clip, paused graph, LOD8, graph.
Native Encore fixture; timings are CPU updates, not rendered editor/game FPS.
"""
from pathlib import Path
import re
import subprocess
import sys

repo = Path(__file__).resolve().parents[3]
stage = repo / 'target/animation/graph-crowd'
(stage / 'src').mkdir(parents=True, exist_ok=True)
seen = set()


def copy_module(path):
    path = path.resolve()
    if path in seen:
        return
    seen.add(path)
    source = path.read_text()
    dest = stage / 'src' / path.relative_to(repo / 'src')
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(source)
    for prefix, tail in re.findall(r'(?:pub\s+)?import\s+(mod|refrain)::([^\s\n{]+)', source):
        pieces = tail.rstrip(':').split('::')
        base = path.parent if prefix == 'mod' else repo / 'src'
        while pieces:
            stem = base.joinpath(*pieces)
            found = next((p for p in [stem.with_suffix('.enq'), stem / 'mod.enq'] if p.is_file()), None)
            if found:
                copy_module(found)
                break
            pieces.pop()


copy_module(repo / 'src/animation/runtime.enq')
(stage / 'src/main.enq').write_text((repo / 'tests/animation/graph_crowd.enq').read_text().replace('graphene::', 'mod::'))
(stage / 'encore.toml').write_text('[project]\nname="graphene-animation-graph-crowd"\nversion="0.0.0"\ndependencies=["index@std@0.0.5"]\n')
chunks = (repo / 'encore.lock').read_text().split('[[packages]]')
(stage / 'encore.lock').write_text(chunks[0] + ''.join('[[packages]]' + c for c in chunks[1:] if any(f'name = "{n}"' in c for n in ['std', 'core', 'platform'])))
compiler = Path(sys.argv[1]) if len(sys.argv) > 1 else repo.parent / 'Encore/encore/target/release/encore'
for args in [['check'], ['build', '--profile', 'release', '--jobs', '2']]:
    subprocess.run([str(compiler), *args], cwd=stage, check=True)
subprocess.run([str(stage / 'target/release/graphene-animation-graph-crowd')], cwd=repo, check=True)
print('Animation graph crowds: PASS (CPU-only benchmark)')
