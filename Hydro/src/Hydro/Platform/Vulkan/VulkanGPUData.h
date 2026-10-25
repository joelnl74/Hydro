#pragma once
#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>

// Structs the shaders read, mirrored in C++. Rules (plan M2, "GPU data layout"):
//  - uniform blocks use std140, storage buffers std430, push constants std430;
//  - fields are vec4-sized: a vec3 is followed by a float that uses its padding, never by another vec3;
//  - every struct has static_asserts on its size and offsets, so a C++ / GLSL mismatch fails the build
//    instead of shading wrong.
// Descriptor sets: set 0 = the frame / view (camera; later lights), set 1 = the material (M4); per-object
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
