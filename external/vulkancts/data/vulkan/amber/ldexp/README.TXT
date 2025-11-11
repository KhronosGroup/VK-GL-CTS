How the Amber files were generated:

1. Wrote template.glsl.

This base shader is a compute shader with 64 invocations that receives 3
buffers.

  a. One to receive the significands (some kind of floating point) for the
     ldexp operation.

  b. Another one that contains the exponents (some kind of integer) for the
     ldexp operation.

  c. Another one to store the results in the same kind of floating point type
     as the significands.

The numbers (the buffer contents) will be the same for scalars, vec2 and vec4,
but when interpreted as scalars, vec2 and vec4 the number of actual operations
varies. As the shader always has 64 invocations, we pass the actual operation
count in a push constant and check the invocation index against it.

We run the ldexp operation and store the result in the results buffer.

2. Run gen_shaders.py.

This generates glsl versions of all shaders for all desired type combinations
of significands and exponents. These are the ldexp*.glsl files.

3. Compile all generated shaders to SPIR-V with gen_spv.sh.

This generates the ldexp*.spv files. Unfortunately, glslangValidator will fail
to compile variants in which the exponent is a 64-bit integer. Keep reading for
the fix to this issue below.

4. Disassemble the generated SPIR-V files with gen_spvasm.sh.

This generates the ldexp*.spvasm files.

5. Manually add the missing variants glslang was not able to compile

Based on the 16-bit exponent variants, we can generate 64-bit ones but we need
to:

  a. Replace the Int16 with the Int64 capability.
  b. Remove unneeded 16-bit storage capabilities if we do not use Float16.
  c. Adjust the stride in the input exponents array.

6. Manually change the generated SPIR-V assembly

glslang likes to cast the exponent to a 32-bit integer sometimes, so we need to
remove and change a couple of instructions to make sure the ldexp operations
have the right exponent type. This applies to almost every generated file.

7. Wrote template.amber

This will be used to generate .amber files based on each of the final .spvasm
files.

8. Run gen_amber.py

This will generate the final .amber files with the right significands,
exponents, buffer contents, shaders, result checks, etc.

