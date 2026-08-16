#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
generated="$root/target/generated-shaders"
output="$root/src/shaders/mesh_triangle.enq"
mkdir -p "$generated"

glslc -O --target-env=vulkan1.3 -fshader-stage=mesh \
    "$root/shaders/mesh_triangle.mesh" \
    -o "$generated/mesh_triangle.mesh.spv"
spirv-val --target-env vulkan1.3 "$generated/mesh_triangle.mesh.spv"

{
    printf 'import core::vec::Vec\n\n'
    printf 'pub fn mesh_triangle_spirv() -> Vec[u32] {\n'
    printf '    let mut words = Vec[u32]::new()\n'
    od -An -v -tu4 "$generated/mesh_triangle.mesh.spv" |
        awk '{ for (i = 1; i <= NF; i++) printf "    words.push(%s_u32)\n", $i }'
    printf '    ret words\n}\n'
} > "$output"
