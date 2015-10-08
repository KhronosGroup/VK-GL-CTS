#version 430
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

void main() {
	for (int i=0; i<gl_in.length(); ++i) {
		gl_Position = gl_in[i].gl_Position;
		gl_ViewportIndex = int(gl_in[i].gl_Position.z);
		EmitVertex();
	}
	EndPrimitive();
}