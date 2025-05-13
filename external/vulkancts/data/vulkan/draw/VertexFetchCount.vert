#version 430

vec4 positions[3] = vec4[](
    vec4(0.5f, 0.5f, 1.0f, 1.0f),
    vec4(-0.3f, 0.3f, 1.0f, 1.0f),
    vec4(-1.0f, 1.0f, 1.0f, 1.0f)
);

vec4 colors[3] = vec4[](
    vec4(0.0f, 0.0f, 1.0f, 1.0f), // blue
    vec4(0.0f, 1.0f, 0.0f, 1.0f), // green
    vec4(0.0f, 0.0f, 0.0f, 1.0f)  // black
);

layout(location = 0) out vec4 out_color;

out gl_PerVertex {
    vec4 gl_Position;
	float gl_PointSize;
};

void main() {
	gl_Position = positions[gl_VertexIndex];
	gl_PointSize = 1.0f;
	out_color = colors[gl_VertexIndex];
}