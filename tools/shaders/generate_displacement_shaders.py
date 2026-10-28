#!/usr/bin/env python3
"""Compile shared GPU refinement and both procedural rendering backends."""
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / 'target/generated-shaders'
out.mkdir(parents=True, exist_ok=True)
source = 'import core::vec::Vec\n\n'
for name, symbol, flags in [('refine.comp', 'displacement_refine_spirv', ()),
                     ('ray_proxy.comp', 'displacement_ray_proxy_spirv', ()),
                     ('ray_proxy.comp', 'terrain_ray_proxy_spirv', ('GRAPHENE_TERRAIN',)),
                     ('topology.comp', 'displacement_topology_spirv', ()),
                     ('pull.vert', 'displacement_vertex_spirv', ()),
                     ('pull.mesh', 'displacement_mesh_spirv', ()),
                     ('debug.frag', 'displacement_debug_spirv', ()),
                     ('triangles.frag', 'displacement_triangle_spirv', ()),
                     ('triangles.frag', 'displacement_triangle_linear_spirv', ('GRAPHENE_LINEAR_COLOR',)),
                     ('triangles.frag', 'displacement_triangle_temporal_spirv', ('GRAPHENE_LINEAR_COLOR', 'GRAPHENE_TEMPORAL')),
                     ('selection.frag', 'displacement_selection_spirv', ()),
                     ('skin_source.comp', 'displacement_skin_source_spirv', ())]:
    binary = out / (symbol + '.spv')
    subprocess.run(['glslc', '-O', '--target-env=vulkan1.2',
                    *['-D' + flag for flag in flags], '-I' + str(root / 'shaders/displacement'),
                    str(root / 'shaders/displacement' / name), '-o', str(binary)], check=True)
    subprocess.run(['spirv-val', '--target-env', 'vulkan1.2', str(binary)], check=True)
    data = binary.read_bytes()
    words = struct.unpack('<' + 'I' * (len(data) // 4), data)
    source += 'pub fn ' + symbol + '() -> Vec[u32] {\n    let mut words = Vec[u32]::new()\n'
    source += ''.join('    words.push(' + str(w) + '_u32)\n' for w in words) + '    ret words\n}\n'
(root / 'src/shaders/displacement.enq').write_text(source)
subprocess.run(['python3', str(root / 'tools/shaders/pack_shader_tables.py')], check=True)
