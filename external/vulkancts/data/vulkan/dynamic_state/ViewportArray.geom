#version 430
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 1) in vec4 in_color[];
layout(location = 1) out vec4 out_color;

void main() {
	for (int i=0; i<gl_in.length(); ++i) {
		gl_Position = gl_in[i].gl_Position;
		gl_ViewportIndex = int(gl_in[i].gl_Position.z);
		out_color = in_color[i];
		EmitVertex();
	}
	EndPrimitive();
}