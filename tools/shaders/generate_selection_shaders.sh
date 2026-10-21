#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
generated="$root/target/generated-shaders"
mkdir -p "$generated"
output="$root/src/shaders/selection.enq"
printf 'import core::vec::Vec\n\n' > "$output"
for stage in selection_mask.vert selection_mask.frag selection_outline.vert selection_outline.frag; do
    glslc -O --target-env=vulkan1.2 "$root/shaders/$stage" -o "$generated/$stage.spv"
    spirv-val --target-env vulkan1.2 "$generated/$stage.spv"
    name=$(printf '%s' "$stage" | tr . _)
    {
        printf 'pub fn %s_spirv() -> Vec[u32] {\n    let mut words = Vec[u32]::new()\n' "$name"
        od -An -v -tu4 "$generated/$stage.spv" | awk '{ for (i=1; i<=NF; i++) printf "    words.push(%s_u32)\n", $i }'
        printf '    ret words\n}\n\n'
    } >> "$output"
done

python3 "$root/tools/shaders/pack_shader_tables.py"
