#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
generated="$root/target/generated-shaders"
output="$root/src/shaders/virtual_geometry.enq"
mkdir -p "$generated"

glslc -O --target-env=vulkan1.3 -fshader-stage=mesh \
    "$root/shaders/virtual_geometry.mesh" \
    -o "$generated/virtual_geometry.mesh.spv"
spirv-val --target-env vulkan1.3 "$generated/virtual_geometry.mesh.spv"

{
    printf 'import core::vec::Vec\n\n'
    printf 'pub fn virtual_geometry_mesh_spirv() -> Vec[u32] {\n'
    printf '    let mut words = Vec[u32]::new()\n'
    od -An -v -tu4 "$generated/virtual_geometry.mesh.spv" |
        awk '{ for (i = 1; i <= NF; i++) printf "    words.push(%s_u32)\n", $i }'
    printf '    ret words\n}\n'
} > "$output"
