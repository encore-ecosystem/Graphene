#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
generated="$root/target/generated-shaders"
mkdir -p "$generated"

generate() {
    source=$1
    stage=$2
    module=$3
    function=$4
    output="$root/src/shaders/$module.enq"
    binary="$generated/$module.spv"
    glslc -O --target-env=vulkan1.3 -fshader-stage="$stage" \
        "$root/shaders/$source" -o "$binary"
    spirv-val --target-env vulkan1.3 "$binary"
    {
        printf 'import core::vec::Vec\n\n'
        printf 'pub fn %s() -> Vec[u32] {\n' "$function"
        printf '    let mut words = Vec[u32]::new()\n'
        od -An -v -tu4 "$binary" |
            awk '{ for (i = 1; i <= NF; i++) printf "    words.push(%s_u32)\n", $i }'
        printf '    ret words\n}\n'
    } > "$output"
}

generate virtual_scene_cull.comp compute virtual_scene_cull virtual_scene_cull_spirv
generate virtual_scene_post_cull.comp compute virtual_scene_post_cull virtual_scene_post_cull_spirv
generate virtual_scene.mesh mesh virtual_scene_mesh virtual_scene_mesh_spirv
generate virtual_scene.frag fragment virtual_scene_fragment virtual_scene_fragment_spirv
generate virtual_scene_triangle.frag fragment virtual_scene_triangle virtual_scene_triangle_spirv
