#pragma once
#include <glm/glm.hpp>
#include "Hydro/Renderer/Lighting.h"

#include <cstddef>
#include <cstdint>

// Structs the shaders read, mirrored in C++: std140 uniforms, std430 storage and push constants, vec4-sized fields,
// static_asserts on sizes and offsets. Set 0 = the view, set 1 = the material; per-object data in push constants.
namespace Hydro
{
	// std140; `UniformBufferObject` in vertex.vert, spike_box.vert (binding 0 of set 0).
	struct UniformBufferObject
	{
		alignas(16) glm::mat4 mvp;
	};
	static_assert(sizeof(UniformBufferObject) == 64, "UniformBufferObject: must match the std140 block in vertex.vert");
	static_assert(offsetof(UniformBufferObject, mvp) == 0, "UniformBufferObject: must match the std140 block in vertex.vert");

	// std140; `FrameLighting` in frame_lighting.glsl (binding 2 of set 0): the view's forward lighting and sun shadow
	// map. Counts.y = 0: not forward lit (shaders draw unlit); Counts.z = 1: binding 4 holds the sun shadow map.
	struct FrameLightingUniform
	{
		glm::vec4 CameraPosition{ 0.0f };   // xyz
		glm::vec4 SunDirection{ 0.0f };     // xyz towards the sun (normalised), w = 1 when there is a sun
		glm::vec4 SunColor{ 0.0f };         // rgb times the intensity
		glm::vec4 SkyAmbient{ 0.0f };       // rgb
		glm::vec4 GroundAmbient{ 0.0f };    // rgb
		glm::vec4 FogColor{ 0.0f };         // rgb, w = 1 when there is fog
		glm::vec4 FogParams{ 0.0f };        // x start, y end (metres from FogOrigin), z = falloff bands (0 = smooth)
		glm::ivec4 Counts{ 0 };             // x = lights in the storage buffer, y = 1 when forward lit, z = 1 with a sun
		                                    // shadow map, w = 1 when lighting snaps to texel centres (PixelSnap)
		glm::vec4 FogOrigin{ 0.0f };        // xyz: where fog distance is measured from (what the camera looks at); w seconds
		                                    // (Time::GetUptime, wrapped every hour), set lit or not, for animated materials
		glm::mat4 ShadowMatrix{ 1.0f };     // world -> the sun shadow map's clip space (xy -1..1, z 0..1)
		glm::vec4 ShadowParams{ 0.0f };     // x strength, y texel in UV, z = 1 soft (3 x 3), w normal offset (metres)
	};
	static_assert(sizeof(FrameLightingUniform) == 224, "FrameLightingUniform: must match the std140 block in frame_lighting.glsl");
	static_assert(offsetof(FrameLightingUniform, FogParams) == 96 && offsetof(FrameLightingUniform, Counts) == 112
		&& offsetof(FrameLightingUniform, FogOrigin) == 128 && offsetof(FrameLightingUniform, ShadowMatrix) == 144
		&& offsetof(FrameLightingUniform, ShadowParams) == 208,
		"FrameLightingUniform: must match the std140 block in frame_lighting.glsl");

	// std430; one element of `Lights` in lighting.glsl (binding 3 of set 0, fragment): a point or spot light.
	struct GPULight
	{
		glm::vec4 PositionRange{ 0.0f };     // xyz position, w range
		glm::vec4 ColorType{ 0.0f };         // rgb colour times intensity, w = 0 point, 1 spot
		glm::vec4 DirectionCosInner{ 0.0f }; // xyz spot direction, w cos(inner angle)
		glm::vec4 Params{ 0.0f };            // x cos(outer angle)
	};
	static_assert(sizeof(GPULight) == 64, "GPULight: must match the std430 struct in lighting.glsl");
	static_assert(offsetof(GPULight, Params) == 48, "GPULight: must match the std430 struct in lighting.glsl");

	// Point and spot lights per view (LIGHT_CAPACITY in lighting.glsl). Beyond it, those reaching least near the
	// camera's focus are dropped.
	static constexpr uint32_t MaxForwardLights = MaxForwardLightsPerView;

	// Push constants (std430) of the Tonemap pass (tonemap.frag, with vignette and grading #137).
	struct TonemapPushConstants
	{
		// x: 2^Exposure; y: bloom intensity (0 = binding 1 unread); z: 1 = mix in the depth of field (bindings 2 to 4);
		// w: 1 = grade.
		glm::vec4 Params{ 1.0f, 0.0f, 0.0f, 0.0f };
		glm::vec4 Vignette{ 0.0f, 0.55f, 0.5f, 0.0f };    // x: intensity (0 = no vignette), y: radius, z: softness
		glm::vec4 WhiteBalance{ 1.0f, 1.0f, 1.0f, 1.0f }; // rgb: white balance multiplier, w: contrast
		glm::vec4 Lift{ 0.0f, 0.0f, 0.0f, 1.0f };         // rgb: lift, w: saturation
		glm::vec4 InverseGamma{ 1.0f };                   // rgb: 1 / gamma
		glm::vec4 Gain{ 1.0f };                           // rgb: gain
	};
	static_assert(sizeof(TonemapPushConstants) == 96, "TonemapPushConstants: must match the push_constant block in tonemap.frag");

	// Push constants (std430) of the bloom passes: xy = one source texel in UV, zw per pass (prefilter: threshold,
	// knee; up: radius).
	struct BloomPushConstants
	{
		glm::vec4 Params{ 0.0f };
	};
	static_assert(sizeof(BloomPushConstants) == 16, "BloomPushConstants: must match the push_constant blocks of the bloom shaders");

	// Push constants (std430) of the depth of field passes (dof_common.glsl; #135). 112 bytes, within the 128 allowed.
	struct DepthOfFieldPushConstants
	{
		glm::mat4 InverseViewProjection{ 1.0f }; // clip space -> world, for the circle of confusion from depth
		glm::vec4 Focus{ 0.0f };   // xyz: the camera's view direction (unit); w: dot(focus point, direction)
		glm::vec4 Band{ 0.0f };    // x near start, y far start (metres), z 1 / range, w largest radius (full-size pixels)
		glm::vec4 Texel{ 0.0f };   // xy source texel in UV, z largest radius (pass pixels), w gather step
	};
	static_assert(sizeof(DepthOfFieldPushConstants) == 112,
		"DepthOfFieldPushConstants: must match the push_constant block in dof_common.glsl");

	// Push constants (std430) of a drawn object: transform, entity ID (-1 = none), material index and opacity (below 1
	// mesh.frag dithers it away). 80 bytes.
	struct ObjectPushConstants
	{
		glm::mat4 Transform{ 1.0f };
		int32_t EntityID = -1;
		uint32_t MaterialIndex = 0;
		float Opacity = 1.0f;
		uint32_t Padding = 0;
	};
	static_assert(sizeof(ObjectPushConstants) == 80, "ObjectPushConstants: must match the push_constant block of the mesh shaders");
	static_assert(offsetof(ObjectPushConstants, Transform) == 0, "ObjectPushConstants: Transform at 0");
	static_assert(offsetof(ObjectPushConstants, EntityID) == 64, "ObjectPushConstants: EntityID at 64");
	static_assert(offsetof(ObjectPushConstants, MaterialIndex) == 68, "ObjectPushConstants: MaterialIndex at 68");
	static_assert(offsetof(ObjectPushConstants, Opacity) == 72, "ObjectPushConstants: Opacity at 72");
}
