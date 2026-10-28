#!/usr/bin/env python3
"""Portable shaders remain feature-free; these variants require hardware queries."""
from pathlib import Path
import struct
import subprocess
root = Path(__file__).resolve().parents[2]
out = root / 'target/generated-shaders'
out.mkdir(parents=True, exist_ok=True)
source = 'import core::vec::Vec\n\n'
for family in ['material', 'virtual', 'terrain']:
    for mode in ['sdr', 'linear', 'temporal']:
        symbol = 'ray_' + family + '_' + mode
        flags = ['GRAPHENE_RAY_LIGHTING', 'GRAPHENE_ENVIRONMENT_SHELTER']
        if family != 'virtual': flags += ['GRAPHENE_BASE_COLOR_TEXTURE']
        if family == 'terrain': flags += ['GRAPHENE_TERRAIN']
        if mode != 'sdr': flags += ['GRAPHENE_LINEAR_COLOR']
        if mode == 'temporal': flags += ['GRAPHENE_TEMPORAL']
        binary = out / (symbol + '.spv')
        subprocess.run(['glslc', '-O', '--target-env=vulkan1.2',
            *['-D' + v for v in flags], '-I' + str(root / 'shaders'),
            str(root / 'shaders' / ('virtual_scene.frag' if family == 'virtual' else 'cube.frag')),
            '-o', str(binary)], check=True)
        subprocess.run(['spirv-val', '--target-env', 'vulkan1.2', str(binary)], check=True)
        data = binary.read_bytes()
        words = struct.unpack('<' + 'I' * (len(data) // 4), data)
        source += 'pub fn ' + symbol + '() -> Vec[u32] {\n    let mut words = Vec[u32]::new()\n'
        source += ''.join('    words.push(' + str(w) + '_u32)\n' for w in words) + '    ret words\n}\n'
(root / 'src/shaders/ray_lighting.enq').write_text(source)
subprocess.run(['python3', str(root / 'tools/shaders/pack_shader_tables.py')], check=True)
