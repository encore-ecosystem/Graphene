#!/usr/bin/env python3
"""Build deterministic SPIR-V and checked-in Metal shader artifacts.

The editor never invokes shaderc, SPIRV-Cross, or the Metal compiler at
runtime. This developer tool translates the portable baseline ahead of time
and embeds each one-entry-point metallib in Luma's native Metal package.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import struct
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


EXPECTED_SPIRV_CROSS = "1.4.357.0"
SPIRV_MAGIC = 0x07230203
FNV_OFFSET = 1469598103934665603
FNV_PRIME = 1099511628211


@dataclass(frozen=True)
class Shader:
    name: str
    source: str
    stage: str
    target_env: str
    output: str
    function: str
    threadgroup_x: int = 1


SHADERS = (
    Shader("triangle_vertex", "triangle.vert", "vert", "vulkan1.2", "triangle.enq", "triangle_vertex_spirv"),
    Shader("triangle_fragment", "triangle.frag", "frag", "vulkan1.2", "triangle.enq", "triangle_fragment_spirv"),
    Shader("depth_vertex", "depth.vert", "vert", "vulkan1.2", "depth.enq", "depth_vertex_spirv"),
    Shader("depth_fragment", "depth.frag", "frag", "vulkan1.2", "depth.enq", "depth_fragment_spirv"),
    Shader("cube_vertex", "cube.vert", "vert", "vulkan1.2", "cube.enq", "cube_vertex_spirv"),
    Shader("cube_fragment", "cube.frag", "frag", "vulkan1.2", "cube.enq", "cube_fragment_spirv"),
    Shader("double_compute", "double.comp", "comp", "vulkan1.2", "compute.enq", "double_words", 1),
    Shader("cluster_cull", "cluster_cull.comp", "comp", "vulkan1.2", "cluster_cull.enq", "cluster_cull_spirv", 64),
    Shader("software_gi", "software_gi.comp", "comp", "vulkan1.2", "software_gi.enq", "software_gi_spirv", 64),
    Shader("gi_spatial_filter", "gi_spatial_filter.comp", "comp", "vulkan1.3", "gi_spatial_filter.enq", "gi_spatial_filter_spirv", 64),
)


UI_SHADERS = (
    ("luma_ui_vertex", "ui.vert", "vert"),
    ("luma_ui_fragment", "ui.frag", "frag"),
)


EXPECTED_MSL_BINDINGS = {
    "triangle_vertex": ("[[buffer(0)]]",),
    "triangle_fragment": ("[[texture(1)]]", "[[sampler(1)]]"),
    "cube_vertex": ("[[buffer(0)]]", "[[buffer(1)]]"),
    "cube_fragment": (
        "[[buffer(0)]]",
        "[[buffer(2)]]",
        "[[buffer(3)]]",
        "[[buffer(4)]]",
    ),
    "double_compute": ("[[buffer(0)]]",),
    "cluster_cull": ("[[buffer(0)]]",),
    "software_gi": ("[[buffer(0)]]", "[[buffer(1)]]", "[[buffer(2)]]"),
    "gi_spatial_filter": (
        "[[buffer(0)]]",
        "[[buffer(1)]]",
        "[[buffer(2)]]",
        "[[buffer(3)]]",
    ),
    "luma_ui_fragment": ("[[texture(0)]]", "[[sampler(0)]]"),
}


def run(*arguments: str, cwd: Path | None = None) -> str:
    result = subprocess.run(
        arguments,
        cwd=cwd,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    return result.stdout.strip()


def require_tool(name: str) -> str:
    path = shutil.which(name)
    if path is None:
        raise RuntimeError(f"required developer tool is unavailable: {name}")
    return path


def fnv1a64(data: bytes) -> int:
    value = FNV_OFFSET
    for byte in data:
        value ^= byte
        value = (value * FNV_PRIME) & 0xFFFFFFFFFFFFFFFF
    return value


def words(data: bytes) -> tuple[int, ...]:
    if len(data) % 4 != 0:
        raise RuntimeError(f"artifact size {len(data)} is not 32-bit aligned")
    return struct.unpack(f"<{len(data) // 4}I", data)


def artifact_token(data: bytes) -> bytes:
    return struct.pack("<IQ", SPIRV_MAGIC, fnv1a64(data))


def encore_function(name: str, data: bytes) -> str:
    values = words(data)
    lines = [
        '#cfg(target_os = "macos")',
        f"pub fn {name}() -> Vec[u32] {{",
        "    let mut words = Vec[u32]::with_capacity(3_usize)",
    ]
    lines.extend(f"    words.push({value}_u32)" for value in words(artifact_token(data)))
    lines.extend(("    ret words", "}", "", '#cfg(not(target_os = "macos"))'))
    lines.extend((
        f"pub fn {name}() -> Vec[u32] {{",
        f"    let mut words = Vec[u32]::with_capacity({len(values)}_usize)",
    ))
    lines.extend(f"    words.push({value}_u32)" for value in values)
    lines.extend(("    ret words", "}"))
    return "\n".join(lines)


def c_array(name: str, data: bytes) -> str:
    padded = data + bytes((-len(data)) % 4)
    values = struct.unpack(f"<{len(padded) // 4}I", padded)
    lines = [f"static const uint32_t {name}[] = {{"]
    for index in range(0, len(values), 8):
        chunk = ", ".join(f"0x{value:08x}u" for value in values[index : index + 8])
        lines.append(f"    {chunk},")
    lines.append("};")
    lines.append(f"static const size_t {name}_size = {len(data)}u;")
    return "\n".join(lines)


def compile_spirv(glslc: str, root: Path, shader: Shader, output: Path) -> bytes:
    run(
        glslc,
        "-O",
        f"--target-env={shader.target_env}",
        f"-fshader-stage={shader.stage}",
        str(root / "shaders" / shader.source),
        "-o",
        str(output),
    )
    data = output.read_bytes()
    if not data.startswith(struct.pack("<I", SPIRV_MAGIC)):
        raise RuntimeError(f"{shader.source} did not produce SPIR-V")
    return data


def translate_msl(
    spirv_cross: str,
    spirv: Path,
    name: str,
    stage: str,
    output_root: Path,
) -> Path:
    msl = output_root / f"{name}.metal"
    run(
        spirv_cross,
        str(spirv),
        "--msl",
        "--msl-version",
        "30100",
        "--msl-decoration-binding",
        "--rename-entry-point",
        "main",
        name,
        stage,
        "--output",
        str(msl),
    )
    source = msl.read_text()
    keyword = {"vert": "vertex", "frag": "fragment", "comp": "kernel"}[stage]
    if f"{keyword} " not in source or name not in source:
        raise RuntimeError(f"{name} did not produce the expected {keyword} entry point")
    for expected in EXPECTED_MSL_BINDINGS.get(name, ()):
        if expected not in source:
            raise RuntimeError(f"{name} is missing preserved Metal binding {expected}")
    return msl


def compile_metallib(
    spirv_cross: str,
    metal: str,
    metallib: str,
    spirv: Path,
    name: str,
    stage: str,
    output_root: Path,
) -> bytes:
    msl = translate_msl(spirv_cross, spirv, name, stage, output_root)
    air = output_root / f"{name}.air"
    library = output_root / f"{name}.metallib"
    run(
        metal,
        "-std=metal3.1",
        "-mmacosx-version-min=14.0",
        "-c",
        str(msl),
        "-o",
        str(air),
    )
    run(metallib, str(air), "-o", str(library))
    return library.read_bytes()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--luma-root",
        type=Path,
        default=Path(__file__).resolve().parents[2] / "luma",
    )
    parser.add_argument(
        "--validate-only",
        action="store_true",
        help="compile GLSL and validate generated MSL without requiring Xcode",
    )
    arguments = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    luma = arguments.luma_root.resolve()
    metal_native = luma / "workspace" / "metal_native"
    if not (metal_native / "backend.m").is_file():
        raise RuntimeError(f"Luma Metal package was not found at {metal_native}")

    glslc = require_tool("glslc")
    spirv_cross = require_tool("spirv-cross")
    brew = require_tool("brew")
    installed_cross = run(brew, "list", "--versions", "spirv-cross")
    if installed_cross != f"spirv-cross {EXPECTED_SPIRV_CROSS}":
        raise RuntimeError(
            f"expected spirv-cross {EXPECTED_SPIRV_CROSS}, found {installed_cross or 'nothing'}"
        )
    generated = root / "target" / "generated-metal-shaders"
    generated.mkdir(parents=True, exist_ok=True)
    if arguments.validate_only:
        for shader in SHADERS:
            spirv_path = generated / f"{shader.name}.spv"
            compile_spirv(glslc, root, shader, spirv_path)
            translate_msl(
                spirv_cross, spirv_path, shader.name, shader.stage, generated
            )
        ui_root = luma / "workspace" / "vulkan_native" / "shaders"
        for name, source, stage in UI_SHADERS:
            spirv_path = generated / f"{name}.spv"
            run(
                glslc,
                "-O",
                "--target-env=vulkan1.2",
                f"-fshader-stage={stage}",
                str(ui_root / source),
                "-o",
                str(spirv_path),
            )
            translate_msl(spirv_cross, spirv_path, name, stage, generated)
        print(f"Validated {len(SHADERS)} Graphene and {len(UI_SHADERS)} Luma MSL shaders")
        return 0

    try:
        metal = run("xcrun", "--find", "metal")
        metallib = run("xcrun", "--find", "metallib")
    except subprocess.CalledProcessError as error:
        raise RuntimeError(
            "Xcode's optional Metal toolchain is unavailable; run "
            "`xcodebuild -downloadComponent metalToolchain` after installing Xcode"
        ) from error

    compiled: list[tuple[Shader, bytes, bytes]] = []
    for shader in SHADERS:
        spirv_path = generated / f"{shader.name}.spv"
        spirv_data = compile_spirv(glslc, root, shader, spirv_path)
        metal_data = compile_metallib(
            spirv_cross, metal, metallib, spirv_path, shader.name, shader.stage, generated
        )
        compiled.append((shader, spirv_data, metal_data))

    by_output: dict[str, list[tuple[Shader, bytes]]] = {}
    for shader, spirv_data, _ in compiled:
        by_output.setdefault(shader.output, []).append((shader, spirv_data))
    for output, modules in by_output.items():
        source = "import core::vec::Vec\n\n" + "\n\n".join(
            encore_function(shader.function, data) for shader, data in modules
        )
        (root / "src" / "shaders" / output).write_text(source + "\n")

    arrays = [c_array(f"graphene_{shader.name}_metallib", data) for shader, _, data in compiled]
    lookup = [
        "/* Generated by Graphene tools/generate_metal_shaders.py. */",
        *arrays,
        "static bool graphene_metal_shader_artifact(uint64_t spirv_hash, uint32_t stage,",
        "    const uint32_t **words, size_t *byte_count, uint32_t *threadgroup_x) {",
    ]
    for shader, spirv_data, metal_data in compiled:
        stage = {"vert": 1, "frag": 16, "comp": 32}[shader.stage]
        array = f"graphene_{shader.name}_metallib"
        lookup.extend(
            (
                f"    if (spirv_hash == 0x{fnv1a64(artifact_token(spirv_data)):016x}ull && stage == {stage}u) {{",
                f"        *words = {array};",
                f"        *byte_count = {array}_size;",
                f"        *threadgroup_x = {shader.threadgroup_x}u;",
                "        return true;",
                "    }",
            )
        )
    lookup.extend(("    return false;", "}"))
    (metal_native / "graphene_metallib.inc").write_text("\n".join(lookup) + "\n")

    ui_arrays = []
    ui_root = luma / "workspace" / "vulkan_native" / "shaders"
    for name, source, stage in UI_SHADERS:
        spirv_path = generated / f"{name}.spv"
        run(
            glslc,
            "-O",
            "--target-env=vulkan1.2",
            f"-fshader-stage={stage}",
            str(ui_root / source),
            "-o",
            str(spirv_path),
        )
        library = compile_metallib(
            spirv_cross, metal, metallib, spirv_path, name, stage, generated
        )
        ui_arrays.append(c_array(f"{name}_metallib", library))
    (metal_native / "ui_metallib.inc").write_text(
        "/* Generated by Graphene tools/generate_metal_shaders.py. */\n"
        + "\n\n".join(ui_arrays)
        + "\n"
    )

    metadata = {
        "format": 1,
        "target": "aarch64-apple-darwin",
        "minimum_macos": "14.0",
        "metal_language": "3.1",
        "spirv_cross": EXPECTED_SPIRV_CROSS,
        "shaderc": run(glslc, "--version").splitlines()[0],
        "xcode": run("xcodebuild", "-version").splitlines(),
        "shaders": [
            {
                "name": shader.name,
                "source": shader.source,
                "stage": shader.stage,
                "spirv_sha256": hashlib.sha256(spirv_data).hexdigest(),
                "metallib_sha256": hashlib.sha256(metal_data).hexdigest(),
            }
            for shader, spirv_data, metal_data in compiled
        ],
    }
    metadata_root = root / "shaders" / "metal"
    metadata_root.mkdir(parents=True, exist_ok=True)
    (metadata_root / "artifacts.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n"
    )
    print(f"Generated {len(compiled)} Graphene and {len(UI_SHADERS)} Luma Metal shaders")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"generate_metal_shaders: {error}", file=sys.stderr)
        raise SystemExit(1)
