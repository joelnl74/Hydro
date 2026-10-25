#version 450

// TEMPORARY(S1, removed in M4): the spike box, unlit: its face shade times its colour.

layout(location = 0) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

void main()
{
	outColor = fragColor;
}
