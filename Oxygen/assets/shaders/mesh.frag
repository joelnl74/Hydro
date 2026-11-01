#version 450

#extension GL_GOOGLE_include_directive : require

#include "lighting.glsl"

// The Standard material (plan M4): albedo * tint, lit by the view's forward lighting when it has it
// (plan M6; unlit otherwise), plus emission, then fog; cut out below AlphaCutoff.

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) flat in int entityID;
layout(location = 4) in vec3 fragWorldPos;

// Must match MaterialUniform in Renderer3D.cpp (std140, 48 bytes).
layout(set = 1, binding = 0) uniform Material {
    vec4 tint;
    vec4 emission;   // rgb = colour * intensity
    vec2 tiling;     // repeats per metre
    float alphaCutoff;
    float padding;
} material;
// The same block as mesh.vert's (ObjectPushConstants in VulkanGPUData.h); only the opacity is read here.
layout(push_constant) uniform ObjectPushConstants {
    mat4 transform;
    int entityID;
    uint materialIndex;
    float opacity;
    uint padding;
} object;

layout(set = 1, binding = 1) uniform sampler2D albedoTexture;
layout(set = 1, binding = 2) uniform sampler2D emissiveTexture;

layout(location = 0) out vec4 outColor;
// The Editor view's entity-ID attachment; ignored in views without it.
layout(location = 1) out int outEntityID;

// A 4 x 4 ordered dither: a mesh at opacity a keeps a of its pixels in a fixed screen pattern (screen-door
// transparency), so it fades without sorting and its depth stays right for what is behind it.
float DitherThreshold(vec2 pixel)
{
	const float bayer[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);
	ivec2 p = ivec2(mod(pixel, 4.0));
	return (bayer[p.y * 4 + p.x] + 0.5) / 16.0;
}

void main()
{
	if (object.opacity < 1.0 && object.opacity <= DitherThreshold(gl_FragCoord.xy))
	{
		discard;
	}
	vec2 uv = fragUV * material.tiling;
	vec4 albedo = texture(albedoTexture, uv) * material.tint * fragColor;
	if (albedo.a < material.alphaCutoff)
	{
		discard;
	}
	vec3 emissive = texture(emissiveTexture, uv).rgb * material.emission.rgb;
	if (IsForwardLit() == false)
	{
		outColor = vec4(albedo.rgb + emissive, 1.0);
		outEntityID = entityID;
		return;
	}

	// A normal that faces away from the camera belongs to a back face seen through a cut-out: flip it.
	vec3 normal = normalize(fragNormal);
	if (gl_FrontFacing == false)
	{
		normal = -normal;
	}
	vec3 lit = albedo.rgb * ComputeLighting(fragWorldPos, normal, 0.5, SunShadowSurface(fragWorldPos, normal)) + emissive;
	outColor = vec4(ApplyFog(lit, fragWorldPos), 1.0);
	outEntityID = entityID;
}
