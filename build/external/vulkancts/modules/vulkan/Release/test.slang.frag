#version 310 es
layout(location = 0) out mediump vec4 dEQP_FragColor;
layout(std140, set = 0, binding = 0) uniform buffer0 { highp mat4x2 u_in0; };
layout(std140, set = 0, binding = 1) uniform buffer1 { highp mat2x4 u_in1; };

void main (void)
{
	highp mat2 res = u_in0 * u_in1;
	dEQP_FragColor = vec4(res[0][0], res[1][0], res[0][1]+res[1][1], 1.0);
}
