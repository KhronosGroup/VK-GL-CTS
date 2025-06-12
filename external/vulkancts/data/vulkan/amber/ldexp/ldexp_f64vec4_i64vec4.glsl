#version 460
#extension GL_EXT_shader_explicit_arithmetic_types : enable

layout (local_size_x=64, local_size_y=1, local_size_z=1) in;

layout (set=0, binding=0, std430) readonly buffer SignificandBlock {
    f64vec4 significands[];
};
layout (set=0, binding=1, std430) readonly buffer ExponentsBlock {
    i16vec4 exponents[];
};
layout (set=0, binding=2, std430) buffer ResultsBlock {
    f64vec4 results[];
};

layout (push_constant, std430) uniform PushConstantBlock {
    uint count;
};

void main()
{
    const uint idx = gl_LocalInvocationIndex;
    if (idx < count) {
	const f64vec4 s = significands[idx];
	const i16vec4 e = exponents[idx];
	const f64vec4 r = ldexp(s, e);
	results[idx] = r;
    }
}
