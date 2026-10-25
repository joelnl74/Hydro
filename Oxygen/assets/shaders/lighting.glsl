// Forward lighting (plan M6), shared by the lit sprite shaders (fragment_lit.frag, sprite_cutout_lit.frag)
// and the mesh shader (mesh.frag). Set 0 bindings 2 and 3 are the view's, filled every frame by
// Renderer2D::UploadLighting from SceneRenderer's SceneLighting. Included, not compiled on its own.

#include "light_falloff.glsl"

// Must match FrameLightingUniform in VulkanGPUData.h (std140, 144 bytes).
layout(set = 0, binding = 2) uniform FrameLighting {
    vec4 cameraPosition;
    vec4 sunDirection;  // xyz towards the sun (unit length), w = 1 when there is a sun
    vec4 sunColor;      // rgb, times its intensity
    vec4 skyAmbient;    // what surfaces facing up get
    vec4 groundAmbient; // and facing down
    vec4 fogColor;      // w = 1 when fog is on
    vec4 fogParams;     // x start, y end (metres from fogOrigin), z bands of the point lights' falloff
    ivec4 counts;       // x lights, y = 1 when the view is forward lit
    vec4 fogOrigin;     // xyz: what the camera looks at (its point on the ground)
} lighting;

// Must match GPULight in VulkanGPUData.h (std430, 64 bytes).
struct Light {
    vec4 positionRange;     // xyz world position, w radius
    vec4 colorType;         // rgb colour times intensity, w 0 point / 1 spot
    vec4 directionCosInner; // xyz where a spot points, w cos(inner angle)
    vec4 params;            // x cos(outer angle)
};
layout(std430, set = 0, binding = 3) readonly buffer Lights {
    Light lights[];
} lightBuffer;

bool IsForwardLit()
{
	return lighting.counts.y == 1;
}

// The light reaching a surface at `position` with normal `normal`. `wrap` softens the terminator:
// 0 = Lambert, 1 = lit all round (half-lambert style); sprites use more wrap than meshes.
vec3 ComputeLighting(vec3 position, vec3 normal, float wrap)
{
	// Hemisphere ambient: Z is up.
	vec3 light = mix(lighting.groundAmbient.rgb, lighting.skyAmbient.rgb, normal.z * 0.5 + 0.5);

	if (lighting.sunDirection.w > 0.5)
	{
		float ndl = dot(normal, lighting.sunDirection.xyz);
		light += lighting.sunColor.rgb * max((ndl + wrap) / (1.0 + wrap), 0.0);
	}

	float bands = lighting.fogParams.z;
	for (int i = 0; i < lighting.counts.x; i++)
	{
		Light source = lightBuffer.lights[i];
		vec3 toLight = source.positionRange.xyz - position;
		float distanceToLight = length(toLight);
		float range = max(source.positionRange.w, 0.001);
		if (distanceToLight >= range)
		{
			continue;
		}
		vec3 l = toLight / max(distanceToLight, 0.0001);
		float attenuation = LightFalloff(distanceToLight / range, bands);
		if (source.colorType.w > 0.5)
		{
			// A spot: full inside the inner angle, nothing outside the outer one.
			float cosAngle = dot(-l, source.directionCosInner.xyz);
			attenuation *= smoothstep(source.params.x, source.directionCosInner.w, cosAngle);
		}
		float ndl = dot(normal, l);
		light += source.colorType.rgb * attenuation * max((ndl + wrap) / (1.0 + wrap), 0.0);
	}
	return light;
}

// Linear distance fog towards the fog colour, by the distance from what the camera looks at (not from the
// camera itself, so how far away the camera is doesn't matter).
vec3 ApplyFog(vec3 color, vec3 position)
{
	if (lighting.fogColor.w < 0.5)
	{
		return color;
	}
	float distanceToFocus = length(position - lighting.fogOrigin.xyz);
	float range = max(lighting.fogParams.y - lighting.fogParams.x, 0.001);
	float amount = clamp((distanceToFocus - lighting.fogParams.x) / range, 0.0, 1.0);
	return mix(color, lighting.fogColor.rgb, amount);
}

// A quad's normal from its world-position derivatives, turned to face the camera (a sprite has no
// back, and is drawn either way round).
vec3 QuadNormal(vec3 position)
{
	vec3 normal = normalize(cross(dFdx(position), dFdy(position)));
	if (dot(normal, lighting.cameraPosition.xyz - position) < 0.0)
	{
		normal = -normal;
	}
	return normal;
}
