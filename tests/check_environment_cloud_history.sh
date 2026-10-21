#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p target/generated-shaders
glslc -O --target-env=vulkan1.2 shaders/aa_fullscreen.vert -o target/generated-shaders/aa_fullscreen.spv
glslc -O --target-env=vulkan1.2 -DGRAPHENE_CLOUD_ONLY -Ishaders shaders/environment_sky.frag -o target/generated-shaders/cloud-history.spv
spirv-val --target-env vulkan1.2 target/generated-shaders/cloud-history.spv
cc -std=c11 -Wall -Wextra -Werror -c tests/environment_cloud_history.c -o target/generated-shaders/cloud-history.o
cc -std=c11 target/generated-shaders/cloud-history.o ../Vulkan/runtime.c -ldl -o target/generated-shaders/cloud-history-test
target/generated-shaders/cloud-history-test target/generated-shaders/cloud-history.spv
