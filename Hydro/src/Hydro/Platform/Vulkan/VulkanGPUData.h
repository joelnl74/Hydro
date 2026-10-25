#pragma once
#include <glm/glm.hpp>
#include "Hydro/Renderer/Lighting.h"

#include <cstddef>
#include <cstdint>

// Structs the shaders read, mirrored in C++. Rules (plan M2, "GPU data layout"):
//  - uniform blocks use std140, storage buffers std430, push constants std430;
//  - fields are vec4-sized: a vec3 is followed by a float that uses its padding, never by another vec3;
//  - every struct has static_asserts on its size and offsets, so a C++ / GLSL mismatch fails the build
//    instead of shading wrong.
// Descriptor sets: set 0 = the frame / view (camera, texture array, forward lighting, lights), set 1 = the material (M4); per-object
// data (transform, entity ID, material index) goes in push constants. The 2D quad shader keeps its texture
// array in set 0 until materials exist.
namespace Hydro
{
	// std140; `UniformBufferObject` in vertex.vert, spike_box.vert (binding 0 of set 0).
	struct UniformBufferObject
	{
		alignas(16) glm::mat4 mvp;
	};
	static_assert(sizeof(UniformBufferObject) == 64, "UniformBufferObject: must match the std140 block in vertex.vert");
	static_assert(offsetof(UniformBufferObject, mvp) == 0, "UniformBufferObject: must match the std140 block in vertex.vert");

	// std140; `FrameLighting` in lighting.glsl (binding 2 of set 0, vertex and fragment): the view's forward
	// lighting this frame (plan M6). Counts.y = 0 means the view isn't forward lit: shaders draw unlit.
	struct FrameLightingUniform
	{
		glm::vec4 CameraPosition{ 0.0f };   // xyz
		glm::vec4 SunDirection{ 0.0f };     // xyz towards the sun (normalised), w = 1 when there is a sun
		glm::vec4 SunColor{ 0.0f };         // rgb times the intensity
		glm::vec4 SkyAmbient{ 0.0f };       // rgb
		glm::vec4 GroundAmbient{ 0.0f };    // rgb
		glm::vec4 FogColor{ 0.0f };         // rgb, w = 1 when there is fog
		glm::vec4 FogParams{ 0.0f };        // x start, y end (metres from the camera), z = falloff bands (0 = smooth)
		glm::ivec4 Counts{ 0 };             // x = lights in the storage buffer, y = 1 when forward lit
	};
	static_assert(sizeof(FrameLightingUniform) == 128, "FrameLightingUniform: must match the std140 block in lighting.glsl");
	static_assert(offsetof(FrameLightingUniform, FogParams) == 96 && offsetof(FrameLightingUniform, Counts) == 112,
		"FrameLightingUniform: must match the std140 block in lighting.glsl");

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

	// Point and spot lights per view (the storage buffer's size; LIGHT_CAPACITY in lighting.glsl). More are
	// dropped, the farthest from the camera first.
	static constexpr uint32_t MaxForwardLights = MaxForwardLightsPerView;

	// Push constants (std430) of a drawn object: its transform, its entity (-1 = none) for the Editor view's
	// entity-ID attachment, and its material's index. 80 bytes, within the 128 Vulkan guarantees. Used by
	// meshes from M4 on.
	struct ObjectPushConstants
	{
		glm::mat4 Transform{ 1.0f };
		int32_t EntityID = -1;
		uint32_t MaterialIndex = 0;
		uint32_t Padding[2] = { 0, 0 };
	};
	static_assert(sizeof(ObjectPushConstants) == 80, "ObjectPushConstants: must match the push_constant block of the mesh shaders");
	static_assert(offsetof(ObjectPushConstants, Transform) == 0, "ObjectPushConstants: Transform at 0");
	static_assert(offsetof(ObjectPushConstants, EntityID) == 64, "ObjectPushConstants: EntityID at 64");
	static_assert(offsetof(ObjectPushConstants, MaterialIndex) == 68, "ObjectPushConstants: MaterialIndex at 68");
}
