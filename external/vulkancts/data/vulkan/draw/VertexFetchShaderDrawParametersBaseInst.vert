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
    vec2 perVertex         = vec2(in_position.x, in_position.y);
    vec2 perInstance[5]    = vec2[5](vec2(0.0, 0.0), vec2(-0.3, 0.0), vec2(0.0, 0.3), vec2(0.5, 0.5), vec2(0.75, -0.8));
    vec4 colors[4]         = vec4[4](vec4(1.0), vec4(0.0, 0.0, 1.0, 1.0), vec4(0.0, 1.0, 0.0, 1.0), vec4(0.0, 1.0, 1.0, 1.0));
    int  baseInstanceIndex = gl_InstanceIndex - gl_BaseInstanceARB;

    gl_Position = vec4(perVertex + perInstance[baseInstanceIndex], 0.0, 1.0);

    if ((gl_VertexIndex - 2) == in_refVertexIndex && gl_DrawIDARB == 0)
        out_color = in_color * colors[baseInstanceIndex];
    else
        out_color = vec4(1.0, 0.0, 0.0, 1.0);
}
