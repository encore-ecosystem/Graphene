#!/usr/bin/env python3
"""Validate and pack terrain's shared PBR and adaptive displacement variants."""
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / 'target/generated-shaders'
out.mkdir(parents=True, exist_ok=True)
source = 'import core::vec::Vec\n\n'
jobs = [('terrain_refine_spirv', 'displacement/refine.comp', ()),
        ('terrain_vertex_spirv', 'displacement/pull.vert', ()),
        ('terrain_fragment_spirv', 'cube.frag', ('GRAPHENE_BASE_COLOR_TEXTURE',)),
        ('terrain_linear_spirv', 'cube.frag', ('GRAPHENE_BASE_COLOR_TEXTURE', 'GRAPHENE_LINEAR_COLOR', 'GRAPHENE_ENVIRONMENT_SHELTER')),
        ('terrain_temporal_spirv', 'cube.frag', ('GRAPHENE_BASE_COLOR_TEXTURE', 'GRAPHENE_LINEAR_COLOR', 'GRAPHENE_ENVIRONMENT_SHELTER', 'GRAPHENE_TEMPORAL'))]
for symbol, filename, flags in jobs:
    binary = out / (symbol + '.spv')
    subprocess.run(['glslc', '-O', '--target-env=vulkan1.2', '-DGRAPHENE_TERRAIN',
                    *['-D' + flag for flag in flags], '-I' + str(root / 'shaders'),
                    str(root / 'shaders' / filename), '-o', str(binary)], check=True)
    subprocess.run(['spirv-val', '--target-env', 'vulkan1.2', str(binary)], check=True)
    data = binary.read_bytes()
    words = struct.unpack('<' + 'I' * (len(data) // 4), data)
    source += 'pub fn ' + symbol + '() -> Vec[u32] {\n    let mut words = Vec[u32]::new()\n'
    source += ''.join('    words.push(' + str(w) + '_u32)\n' for w in words) + '    ret words\n}\n'
(root / 'src/shaders/terrain.enq').write_text(source)
subprocess.run(['python3', str(root / 'tools/shaders/pack_shader_tables.py')], check=True)
