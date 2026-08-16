#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
generated="$root/target/generated-shaders"
output="$root/src/shaders/cluster_cull.enq"
mkdir -p "$generated"

glslc -O --target-env=vulkan1.2 "$root/shaders/cluster_cull.comp" \
    -o "$generated/cluster_cull.comp.spv"
spirv-val --target-env vulkan1.2 "$generated/cluster_cull.comp.spv"

{
    printf 'import core::vec::Vec\n\n'
    printf 'pub fn cluster_cull_spirv() -> Vec[u32] {\n'
    printf '    let mut words = Vec[u32]::new()\n'
    od -An -v -tu4 "$generated/cluster_cull.comp.spv" |
        awk '{ for (i = 1; i <= NF; i++) printf "    words.push(%s_u32)\n", $i }'
    printf '    ret words\n}\n'
} > "$output"
