#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
generated="$root/target/generated-shaders"
output="$root/src/shaders/cube.enq"
mkdir -p "$generated"

glslc -O --target-env=vulkan1.2 "$root/shaders/cube.vert" -o "$generated/cube.vert.spv"
glslc -O --target-env=vulkan1.2 "$root/shaders/cube.frag" -o "$generated/cube.frag.spv"
spirv-val --target-env vulkan1.2 "$generated/cube.vert.spv"
spirv-val --target-env vulkan1.2 "$generated/cube.frag.spv"

emit_function() {
    name=$1
    binary=$2
    printf 'pub fn %s() -> Vec[u32] {\n' "$name"
    printf '    let mut words = Vec[u32]::new()\n'
    od -An -v -tu4 "$binary" | awk '{ for (i = 1; i <= NF; i++) printf "    words.push(%s_u32)\n", $i }'
    printf '    ret words\n}\n'
}

{
    printf 'import core::vec::Vec\n\n'
    emit_function cube_vertex_spirv "$generated/cube.vert.spv"
    printf '\n'
    emit_function cube_fragment_spirv "$generated/cube.frag.spv"
} > "$output"

# Material-aware variant shares the same lighting source and vertex format.
# Existing untextured game pipelines keep their public shader entrypoints.
glslc -O --target-env=vulkan1.2 -DGRAPHENE_BASE_COLOR_TEXTURE "$root/shaders/cube.vert" -o "$generated/material.vert.spv"
glslc -O --target-env=vulkan1.2 -DGRAPHENE_BASE_COLOR_TEXTURE "$root/shaders/cube.frag" -o "$generated/material.frag.spv"
spirv-val --target-env vulkan1.2 "$generated/material.vert.spv"
spirv-val --target-env vulkan1.2 "$generated/material.frag.spv"
{
    printf 'import core::vec::Vec\n\n'
    emit_function material_vertex_spirv "$generated/material.vert.spv"
    printf '\n'
    emit_function material_fragment_spirv "$generated/material.frag.spv"
} > "$root/src/shaders/material.enq"

python3 "$root/tools/shaders/pack_shader_tables.py"
