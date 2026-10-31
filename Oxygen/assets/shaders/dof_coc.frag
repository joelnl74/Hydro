#version 450

#extension GL_GOOGLE_include_directive : require

#include "dof_common.glsl"

// Depth of field, step 1 (#135): the signed circle of confusion of every pixel, from the shared depth. A point is
// measured along the view direction from the camera's focus point on the ground, as lighting.glsl's fog is, but on
// both sides: up to band.x metres in front of it and band.y behind it it stays sharp (0); beyond that the radius
// grows over 1 / band.z metres to band.w pixels. Written into an R16F image of the view's size.

layout(set = 0, binding = 0) uniform sampler2D sceneDepth; // the view's depth (DEPTH32F), read texel for texel

layout(location = 0) in vec2 fragUV;

layout(location = 0) out float outCoc;

void main()
{
	float depth = texelFetch(sceneDepth, ivec2(gl_FragCoord.xy), 0).r;
	// fragUV runs down the view as Vulkan's clip-space y does, so the clip position is fragUV * 2 - 1 (the camera's
	// Y flip is part of the matrix). The cleared depth (the sky) lands on the far plane: as blurred as can be.
	vec4 world = dof.inverseViewProjection * vec4(fragUV * 2.0 - 1.0, depth, 1.0);
	world.xyz /= world.w;

	float distanceToFocus = dot(world.xyz, dof.focus.xyz) - dof.focus.w;
	float start = distanceToFocus < 0.0 ? dof.band.x : dof.band.y;
	float amount = clamp((abs(distanceToFocus) - start) * dof.band.z, 0.0, 1.0);
	outCoc = (distanceToFocus < 0.0 ? -amount : amount) * dof.band.w;
}
