#version 460
#extension GL_EXT_shader_explicit_arithmetic_types : enable

layout (local_size_x=64, local_size_y=1, local_size_z=1) in;

layout (set=0, binding=0, std430) readonly buffer SignificandBlock {
    float64_t significands[];
};
layout (set=0, binding=1, std430) readonly buffer ExponentsBlock {
    int16_t exponents[];
};
layout (set=0, binding=2, std430) buffer ResultsBlock {
    float64_t results[];
};

layout (push_constant, std430) uniform PushConstantBlock {
    uint count;
};

void main()
{
    const uint idx = gl_LocalInvocationIndex;
    if (idx < count) {
	const float64_t s = significands[idx];
	const int16_t e = exponents[idx];
	const float64_t r = ldexp(s, e);
	results[idx] = r;
    }
}
