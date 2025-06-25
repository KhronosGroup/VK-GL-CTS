#!/usr/bin/env bash
for f in ldexp_f*.glsl; do
    glslangValidator -V -o ${f}.spv --target-env vulkan1.0 -S comp ${f}
done