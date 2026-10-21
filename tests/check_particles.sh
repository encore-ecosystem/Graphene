#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p target/vfx
python3 tools/shaders/generate_vfx_shaders.py
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native/vfx/vfx.c tests/vfx/native.c -lm -o target/vfx/native-test
target/vfx/native-test
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native/vfx/fluid.c tests/vfx/fluid.c -lm -o target/vfx/fluid-test
target/vfx/fluid-test
glslc tests/vfx/fluid_uniform.comp -o target/vfx/fluid-uniform-copy.spv
glslc tests/vfx/mrt-opaque.frag -o target/vfx/mrt-opaque.spv
glslc tests/vfx/mrt-translucent.frag -o target/vfx/mrt-translucent.spv
for name in gpu fluid_gpu readback mrt; do
    clang -std=gnu11 -Wall -Wextra -Werror -O2 -c "tests/vfx/$name.c" -o "target/vfx/$name.o"
    clang -std=gnu11 -O2 "target/vfx/$name.o" native/vfx/vfx.c ../Vulkan/runtime.c -ldl -lm -o "target/vfx/$name-test"
    "target/vfx/$name-test"
done
