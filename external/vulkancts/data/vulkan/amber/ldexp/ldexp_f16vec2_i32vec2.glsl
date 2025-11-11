#version 460
#extension GL_EXT_shader_explicit_arithmetic_types : enable

layout (local_size_x=64, local_size_y=1, local_size_z=1) in;

layout (set=0, binding=0, std430) readonly buffer SignificandBlock {
    f16vec2 significands[];
};
layout (set=0, binding=1, std430) readonly buffer ExponentsBlock {
    i32vec2 exponents[];
};
layout (set=0, binding=2, std430) buffer ResultsBlock {
    f16vec2 results[];
};

layout (push_constant, std430) uniform PushConstantBlock {
    uint count;
};

void main()
{
    const uint idx = gl_LocalInvocationIndex;
    if (idx < count) {
	const f16vec2 s = significands[idx];
	const i32vec2 e = exponents[idx];
	const f16vec2 r = ldexp(s, e);
	results[idx] = r;
    }
}
