#!/usr/bin/env python3
"""Build the reproducible Fox + Grass editor showcase from local bundles."""

from pathlib import Path
import shutil
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
BUNDLES = ROOT / "bundles"
WORLD = ROOT / "examples" / "fox_grass"
MESHES = WORLD / "Content" / "Meshes"


def mtl_colors(path: Path) -> dict[str, tuple[float, float, float]]:
    colors: dict[str, tuple[float, float, float]] = {}
    current = ""
    for line in path.read_text().splitlines():
        words = line.split()
        if words[:1] == ["newmtl"]:
            current = words[1]
        elif words[:1] == ["Kd"] and current:
            colors[current] = tuple(map(float, words[1:4]))
    return colors


def bake_fox() -> None:
    source_dir = BUNDLES / "Ultimate Animated Animals" / "OBJ"
    colors = mtl_colors(source_dir / "Fox.mtl")
    positions: list[str] = []
    faces: list[tuple[str, list[int]]] = []
    material = ""
    for line in (source_dir / "Fox.obj").read_text().splitlines():
        words = line.split()
        if words[:1] == ["v"]:
            positions.append(" ".join(words[1:4]))
        elif words[:1] == ["usemtl"]:
            material = words[1]
        elif words[:1] == ["f"]:
            faces.append((material, [int(item.split("/")[0]) for item in words[1:]]))
    output = ["# Fox with MTL base colors baked as OBJ vertex colors"]
    index = 1
    for material_name, face in faces:
        color = colors.get(material_name, (1.0, 1.0, 1.0))
        emitted = []
        for source_index in face:
            output.append(f"v {positions[source_index - 1]} {color[0]} {color[1]} {color[2]}")
            emitted.append(index)
            index += 1
        output.append("f " + " ".join(map(str, emitted)))
    (MESHES / "Fox.obj").write_text("\n".join(output) + "\n")


def build_grass() -> None:
    (MESHES / "GrassGround.obj").write_text("""# 40m grass plane with tiled UVs
v -20 0 -20
v 20 0 -20
v 20 0 20
v -20 0 20
vt 0 0
vt 20 0
vt 20 20
vt 0 20
f 1/1 4/4 3/3 2/2
""")
    source = BUNDLES / "Materials" / "Grass 005" / "Grass005_1K-PNG_Color.png"
    image = Image.open(source).convert("RGBA").resize((512, 512), Image.Resampling.LANCZOS)
    (MESHES / "GrassGround.rgba").write_bytes(image.tobytes())
    (MESHES / "GrassGround.texture").write_text("512 512\n")
    shutil.copy2(source, MESHES / "GrassGround.source.png")


def main() -> None:
    MESHES.mkdir(parents=True, exist_ok=True)
    (WORLD / "game" / "src").mkdir(parents=True, exist_ok=True)
    bake_fox()
    build_grass()


if __name__ == "__main__":
    main()
