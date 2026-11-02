#version 450

#extension GL_GOOGLE_include_directive : require

#include "lighting.glsl"
#include "mesh_standard.glsl"

void main()
{
	ShadeStandardMesh();
}
