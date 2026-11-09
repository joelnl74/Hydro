#version 450

// A triangle that covers the whole view, made from the vertex index (no vertex buffer):
// draw it with vkCmdDraw(3). fragUV is 0..1 across the view.

layout(location = 0) out vec2 fragUV;

void main()
{
	vec2 position = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	fragUV = position;
	gl_Position = vec4(position * 2.0 - 1.0, 0.0, 1.0);
}
