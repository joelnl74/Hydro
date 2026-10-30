#version 450

#extension GL_GOOGLE_include_directive : require

#include "lighting.glsl"

// A terrain's top surface (Terrain plan, "Surface"; scripts/terrain/splat.py is the reference): per art pixel
// (1/32 m) the layer whose painted weight plus edge noise is highest wins, and only its albedo is sampled there. Hard
// pixel borders, no blending. Lit and fogged like mesh.frag. Runs on mesh.vert: fragUV is the position in the terrain's
// own space (TerrainMesher), so the pattern doesn't move with the entity.

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) flat in int entityID;
layout(location = 4) in vec3 fragWorldPos;

// Must match TerrainSurfaceUniform in Renderer3D.cpp (std140, 192 bytes).
layout(set = 1, binding = 0) uniform TerrainSurface {
    vec4 layerTint[4];
    vec4 layerTiling[4]; // xy: repeats per metre
    vec4 roughness;      // per layer, weight units
    vec4 weightMap;      // xy: 1 / metres the weight map covers, z: layer count
    vec4 noise;          // x: cluster size (art pixels), y: share of per-pixel noise
    uvec4 seed;          // x
} surface;
layout(set = 1, binding = 1) uniform sampler2D weightMap; // RGBA: one layer per channel
layout(set = 1, binding = 2) uniform sampler2D layer0;
layout(set = 1, binding = 3) uniform sampler2D layer1;
layout(set = 1, binding = 4) uniform sampler2D layer2;
layout(set = 1, binding = 5) uniform sampler2D layer3;

layout(location = 0) out vec4 outColor;
// The Editor view's entity-ID attachment; ignored in views without it.
layout(location = 1) out int outEntityID;

const float ArtPixelsPerMetre = 32.0;
// Hash tags, as in scripts/terrain/terrain_mesher.py.
const uint TagSplatCluster = 4u;
const uint TagSplatPixel = 5u;

// terrain_mesher.hash_u32, one value at a time.
uint HashStep(uint h, uint value)
{
	h ^= value;
	h *= 0x01000193u;
	h ^= h >> 15;
	h *= 0x2C1B3C6Du;
	h ^= h >> 12;
	h *= 0x297A2D39u;
	h ^= h >> 15;
	return h;
}

// hash01(seed, x, y, layer, tag)
float Hash01(uint seed, int x, int y, int layer, uint tag)
{
	uint h = 0x811C9DC5u;
	h = HashStep(h, seed);
	h = HashStep(h, uint(x));
	h = HashStep(h, uint(y));
	h = HashStep(h, uint(layer));
	h = HashStep(h, tag);
	return float(h) / 4294967296.0;
}

// splat.edge_noise: value noise with a lattice every `cluster` art pixels (smoothstep between), plus a share of
// per-pixel noise. 0..1.
float EdgeNoise(ivec2 pixel, int layer)
{
	uint seed = surface.seed.x;
	float cluster = surface.noise.x;
	vec2 g = vec2(pixel) / cluster;
	vec2 cell = floor(g);
	vec2 f = g - cell;
	f = f * f * (3.0 - 2.0 * f);
	ivec2 c = ivec2(cell);
	float v00 = Hash01(seed, c.x, c.y, layer, TagSplatCluster);
	float v10 = Hash01(seed, c.x + 1, c.y, layer, TagSplatCluster);
	float v01 = Hash01(seed, c.x, c.y + 1, layer, TagSplatCluster);
	float v11 = Hash01(seed, c.x + 1, c.y + 1, layer, TagSplatCluster);
	float clustered = mix(mix(v00, v10, f.x), mix(v01, v11, f.x), f.y);
	float single = Hash01(seed, pixel.x, pixel.y, layer, TagSplatPixel);
	return mix(clustered, single, surface.noise.y);
}

vec4 SampleLayer(int layer, vec2 position)
{
	vec2 uv = position * surface.layerTiling[layer].xy;
	switch (layer)
	{
		case 0: return texture(layer0, uv) * surface.layerTint[0];
		case 1: return texture(layer1, uv) * surface.layerTint[1];
		case 2: return texture(layer2, uv) * surface.layerTint[2];
		default: return texture(layer3, uv) * surface.layerTint[3];
	}
}

void main()
{
	// 1. The art pixel this fragment is in (in the terrain's space) and its centre.
	ivec2 pixel = ivec2(floor(fragUV * ArtPixelsPerMetre));
	vec2 centre = (vec2(pixel) + 0.5) / ArtPixelsPerMetre;

	// 2. The painted weights there (filtered between texels: constant within an art pixel, smooth across the map).
	vec4 weights = texture(weightMap, centre * surface.weightMap.xy);

	// 3. Weight plus edge noise per layer; the highest wins (the first on a tie, like numpy's argmax).
	int layerCount = int(surface.weightMap.z + 0.5);
	int best = 0;
	float bestScore = -1.0e9;
	for (int layer = 0; layer < 4; layer++)
	{
		if (layer >= layerCount)
		{
			break;
		}
		float score = weights[layer] + (EdgeNoise(pixel, layer) - 0.5) * surface.roughness[layer];
		if (score > bestScore)
		{
			bestScore = score;
			best = layer;
		}
	}

	// 4. Only the winner's albedo, at the art pixel's centre.
	vec4 albedo = SampleLayer(best, centre) * fragColor;
	if (IsForwardLit() == false)
	{
		outColor = vec4(albedo.rgb, 1.0);
		outEntityID = entityID;
		return;
	}

	vec3 normal = normalize(fragNormal);
	if (gl_FrontFacing == false)
	{
		normal = -normal;
	}
	vec3 lit = albedo.rgb * ComputeLighting(fragWorldPos, normal, 0.5, SunShadowSurface(fragWorldPos, normal));
	outColor = vec4(ApplyFog(lit, fragWorldPos), 1.0);
	outEntityID = entityID;
}
