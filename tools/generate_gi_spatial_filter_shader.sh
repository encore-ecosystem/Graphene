#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
generated="$root/target/generated-shaders"
mkdir -p "$generated"
binary="$generated/gi_spatial_filter.spv"
output="$root/src/shaders/gi_spatial_filter.enq"

glslc -O --target-env=vulkan1.3 -fshader-stage=compute \
    "$root/shaders/gi_spatial_filter.comp" -o "$binary"
spirv-val --target-env vulkan1.3 "$binary"
{
    printf 'import core::vec::Vec\n\n'
    printf 'pub fn gi_spatial_filter_spirv() -> Vec[u32] {\n'
    printf '    let mut words = Vec[u32]::new()\n'
    od -An -v -tu4 "$binary" |
        awk '{ for (i = 1; i <= NF; i++) printf "    words.push(%s_u32)\n", $i }'
    printf '    ret words\n}\n'
} > "$output"
