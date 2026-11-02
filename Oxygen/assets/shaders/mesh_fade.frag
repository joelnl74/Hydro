#version 450

#extension GL_GOOGLE_include_directive : require

#include "lighting.glsl"
#include "mesh_standard.glsl"
#include "fade_dither.glsl"

// mesh.frag for meshes being faded (MeshComponent::Opacity below 1; Renderer3D picks this variant per draw): the
// Standard material with a screen-door fade by the object's opacity. A game effect kept out of the main shader, so
// it moves to the game once games can register their own shader variants (docs/SceneAndECS.md, Known gaps).

// The same block as mesh.vert's (ObjectPushConstants in VulkanGPUData.h); only the opacity is read here.
layout(push_constant) uniform ObjectPushConstants {
    mat4 transform;
    int entityID;
    uint materialIndex;
    float opacity;
    uint padding;
} object;

void main()
{
	if (FadeDiscards(object.opacity, gl_FragCoord.xy))
	{
		discard;
	}
	ShadeStandardMesh();
}
