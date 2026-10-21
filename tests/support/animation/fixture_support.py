"""Prepare native test inputs without depending on another runner's cache."""
from pathlib import Path
import shutil
import subprocess
import sys


def prepare_runtime_build(repo: Path, stage: Path):
    stage.mkdir(parents=True, exist_ok=True)
    source = (repo / "build.enq").read_text()
    source = source.replace(r'\"runtime.c\"', r'\"' + str(repo / "runtime.c") + r'\"')
    source = source.replace(r'\"link_args\": []', r'\"link_args\": [\"-lm\"]')
    (stage / "build.enq").write_text(source)
    fixture_root = repo / "target/animation"
    fixture_root.mkdir(parents=True, exist_ok=True)
    shutil.copy2(repo / "Assets/Models/Animation/Fox/Fox.glb", fixture_root / "Fox.glb")
    subprocess.run([sys.executable, str(repo / "tests/support/animation/create_fixture.py")], check=True)
