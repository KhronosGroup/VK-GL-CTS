#!/usr/bin/env bash
for f in ldexp*.spv; do
    spirv-dis -o ${f}.spvasm $f
done
