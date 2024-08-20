#version 430

layout(location = 0) in vec4 in_color;
layout(location = 0) out vec4 out_color;
layout (set=0, binding=0, std430) buffer StorageBlock
{
  uint counter;
} ssbo;

void main()
{
  out_color = in_color;
  atomicAdd(ssbo.counter, 1);
}