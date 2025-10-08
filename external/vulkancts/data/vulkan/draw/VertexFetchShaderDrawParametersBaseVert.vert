#version 450 core
#extension GL_ARB_shader_draw_parameters : require

layout(location = 0) in vec4 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in int  in_refVertexIndex;

layout(location = 0) out vec4 out_color;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = vec4(in_position.xy, 0.0, 1.0);

	out_color = vec4(1.0, 0.0, 0.0, 1.0);
    if ((gl_VertexIndex - gl_BaseVertexARB) == in_refVertexIndex && gl_DrawIDARB == 0)
        out_color = in_color;
}
