#version 450
#extension GL_EXT_gpu_shader5 : require
layout(set = 1, binding = 0, std430) buffer AtomicBuffer { highp uint counter[4]; };

layout(points) in;
layout(points, max_vertices = 1) out;
layout(location = 0) flat in highp int vtx_out_index0[];
layout(location = 1) flat in highp int vtx_out_index1[];
layout(location = 2) flat in highp int vtx_out_index2[];
layout(location = 3) flat in highp int vtx_out_index3[];
layout(location = 0) flat out highp uint geom_out_result0;
layout(location = 1) flat out highp uint geom_out_result1;
layout(location = 2) flat out highp uint geom_out_result2;
layout(location = 3) flat out highp uint geom_out_result3;

void main (void)
{
	gl_Position = gl_in[0].gl_Position;
	highp int index0 = vtx_out_index0[0];
	highp int index1 = vtx_out_index1[0];
	highp int index2 = vtx_out_index2[0];
	highp int index3 = vtx_out_index3[0];
	highp uint result0;
	highp uint result1;
	highp uint result2;
	highp uint result3;

	result0 = atomicAdd(counter[index0], uint(1));
	result1 = atomicAdd(counter[index1], uint(1));
	result2 = atomicAdd(counter[index2], uint(1));
	result3 = atomicAdd(counter[index3], uint(1));
	geom_out_result0 = result0;
	geom_out_result1 = result1;
	geom_out_result2 = result2;
	geom_out_result3 = result3;
	EmitVertex();
	EndPrimitive();
}
