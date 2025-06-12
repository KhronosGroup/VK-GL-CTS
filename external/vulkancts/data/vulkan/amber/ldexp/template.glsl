#version 460
#extension GL_EXT_shader_explicit_arithmetic_types : enable

layout (local_size_x=64, local_size_y=1, local_size_z=1) in;

layout (set=0, binding=0, std430) readonly buffer SignificandBlock {{
    {significand_type} significands[];
}};
layout (set=0, binding=1, std430) readonly buffer ExponentsBlock {{
    {exponent_type} exponents[];
}};
layout (set=0, binding=2, std430) buffer ResultsBlock {{
    {significand_type} results[];
}};

layout (push_constant, std430) uniform PushConstantBlock {{
    uint count;
}};

void main()
{{
    const uint idx = gl_LocalInvocationIndex;
    if (idx < count) {{
	const {significand_type} s = significands[idx];
	const {exponent_type} e = exponents[idx];
	const {significand_type} r = ldexp(s, e);
	results[idx] = r;
    }}
}}
