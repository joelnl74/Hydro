// Forward lighting (plan M6) and sun shadows (R14), shared by the lit sprite shaders (fragment_lit.frag,
// sprite_cutout_lit.frag) and the mesh shader (mesh.frag). Set 0 bindings 2, 3 and 4 are the view's, filled
// every frame by Renderer2D from SceneRenderer's SceneLighting. Included, not compiled on its own.

#include "light_falloff.glsl"

#include "frame_lighting.glsl"

// The sun shadow map (R14): depth seen from the sun, compared in the sampler (1 = lit, 0 = in shadow).
// Always bound (an empty map in views without one); read only when counts.z = 1.
layout(set = 0, binding = 4) uniform sampler2DShadow sunShadowMap;

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

// How much sunlight reaches `position` (1 = all, 0 = none, less shadow with a lower strength), looked up
// `offset` metres along `pushDirection` (out of the surface, or towards the sun for a sprite) so a surface
// doesn't shadow itself. Outside the map: lit.
float SunShadow(vec3 position, vec3 pushDirection, float offset)
{
	if (lighting.counts.z == 0)
	{
		return 1.0;
	}
	vec4 clip = lighting.shadowMatrix * vec4(position + pushDirection * offset, 1.0);
	vec3 ndc = clip.xyz / clip.w;
	vec2 uv = ndc.xy * 0.5 + 0.5;
	if (uv.x <= 0.0 || uv.x >= 1.0 || uv.y <= 0.0 || uv.y >= 1.0 || ndc.z >= 1.0)
	{
		return 1.0;
	}

	float texel = lighting.shadowParams.y;
	float visibility = 0.0;
	if (lighting.shadowParams.z > 0.5)
	{
		// Soft: 3 x 3 taps, each blending 2 x 2 texels (linear comparison sampler).
		for (int y = -1; y <= 1; y++)
		{
			for (int x = -1; x <= 1; x++)
			{
				visibility += texture(sunShadowMap, vec3(uv + vec2(x, y) * texel, ndc.z));
			}
		}
		visibility /= 9.0;
	}
	else
	{
		// Hard: the texel's own result (sampled at its centre), so shadow edges are crisp texel steps.
		vec2 snapped = (floor(uv / texel) + 0.5) * texel;
		visibility = texture(sunShadowMap, vec3(snapped, ndc.z));
	}
	return mix(1.0, visibility, lighting.shadowParams.x);
}

// SunShadow for a mesh surface: pushed out along its normal.
float SunShadowSurface(vec3 position, vec3 normal)
{
	return SunShadow(position, normal, lighting.shadowParams.w);
}

// SunShadow for a sprite pixel, looked up at `shadowPosition` (QuadVertex::ShadowPosition: the same texel of
// the quad that casts the sprite's shadow) and pushed towards the sun, past its own entry in the map.
float SunShadowSprite(vec3 shadowPosition)
{
	return SunShadow(shadowPosition, lighting.sunDirection.xyz, lighting.shadowParams.w * 1.5);
}

// The light reaching a surface at `position` with normal `normal`. `wrap` softens the terminator:
// 0 = Lambert, 1 = lit all round (half-lambert style); sprites use more wrap than meshes. `sunVisibility`
// (SunShadow*) scales the sun only: ambient and point and spot lights reach into shadows.
vec3 ComputeLighting(vec3 position, vec3 normal, float wrap, float sunVisibility)
{
	// Hemisphere ambient: Z is up.
	vec3 light = mix(lighting.groundAmbient.rgb, lighting.skyAmbient.rgb, normal.z * 0.5 + 0.5);

	if (lighting.sunDirection.w > 0.5)
	{
		float ndl = dot(normal, lighting.sunDirection.xyz);
		light += lighting.sunColor.rgb * max((ndl + wrap) / (1.0 + wrap), 0.0) * sunVisibility;
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
