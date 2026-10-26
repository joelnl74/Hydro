#include "hypch.h"
#include "VulkanDescriptors.h"

#include "VulkanFrameContext.h"
#include "VulkanUtils.h"
#include "Hydro/Renderer/Renderer.h"

#include <array>
#include <map>

namespace Hydro
{
	struct VulkanDescriptorData
	{
		// Layout cache, keyed by (binding, type, count, stages) of every binding in order.
		std::map<std::vector<std::array<uint32_t, 4>>, VkDescriptorSetLayout> Layouts;

		std::vector<VkDescriptorPool> PersistentPools;
		std::array<std::vector<VkDescriptorPool>, MAX_FRAMES_IN_FLIGHT> FramePools;
		std::array<size_t, MAX_FRAMES_IN_FLIGHT> FramePoolCursor{};

		uint32_t PersistentSets = 0; // live, for the asserts and the shutdown check
	};

	static VulkanDescriptorData* s_Descriptors = nullptr;

	// Sets per pool. Each pool has room for that many sets of any of the layouts the renderer uses (a
	// 32-texture array per set at most); a full pool is followed by a new one.
	static constexpr uint32_t SetsPerPool = 64;

	static VkDescriptorPool CreatePool(bool freeSets)
	{
		const VkDescriptorPoolSize sizes[] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SetsPerPool * 2 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, SetsPerPool * 2 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SetsPerPool * 32 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, SetsPerPool },
			{ VK_DESCRIPTOR_TYPE_SAMPLER, SetsPerPool },
		};

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.flags = freeSets ? VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT : 0;
		poolInfo.maxSets = SetsPerPool;
		poolInfo.poolSizeCount = (uint32_t)(sizeof(sizes) / sizeof(sizes[0]));
		poolInfo.pPoolSizes = sizes;

		VkDescriptorPool pool = VK_NULL_HANDLE;
		VK_CHECK_RESULT(vkCreateDescriptorPool(Renderer::GetVulkanDevice().GetDevice(), &poolInfo, nullptr, &pool));
		return pool;
	}

	// One set from `pool`; false when the pool is full (the caller makes another).
	static bool TryAllocate(VkDescriptorPool pool, VkDescriptorSetLayout layout, VkDescriptorSet& outSet)
	{
		VkDescriptorSetAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.descriptorPool = pool;
		allocateInfo.descriptorSetCount = 1;
		allocateInfo.pSetLayouts = &layout;

		const VkResult result = vkAllocateDescriptorSets(Renderer::GetVulkanDevice().GetDevice(), &allocateInfo, &outSet);
		if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL)
		{
			return false;
		}
		VK_CHECK_RESULT(result);
		return true;
	}

	void VulkanDescriptorAllocator::Init()
	{
		HY_CORE_ASSERT(s_Descriptors == nullptr, "VulkanDescriptorAllocator::Init called twice");
		s_Descriptors = new VulkanDescriptorData();
	}

	void VulkanDescriptorAllocator::Shutdown()
	{
		if (s_Descriptors == nullptr)
		{
			return;
		}

		HY_CORE_ASSERT(s_Descriptors->PersistentSets == 0, "VulkanDescriptorAllocator::Shutdown: persistent descriptor sets were not freed");

		VkDevice device = Renderer::GetVulkanDevice().GetDevice();
		for (VkDescriptorPool pool : s_Descriptors->PersistentPools)
		{
			vkDestroyDescriptorPool(device, pool, nullptr);
		}
		for (std::vector<VkDescriptorPool>& pools : s_Descriptors->FramePools)
		{
			for (VkDescriptorPool pool : pools)
			{
				vkDestroyDescriptorPool(device, pool, nullptr);
			}
		}
		for (auto& [key, layout] : s_Descriptors->Layouts)
		{
			vkDestroyDescriptorSetLayout(device, layout, nullptr);
		}

		delete s_Descriptors;
		s_Descriptors = nullptr;
	}

	VkDescriptorSetLayout VulkanDescriptorAllocator::GetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings)
	{
		std::vector<std::array<uint32_t, 4>> key;
		key.reserve(bindings.size());
		for (const VkDescriptorSetLayoutBinding& binding : bindings)
		{
			HY_CORE_ASSERT(binding.pImmutableSamplers == nullptr, "VulkanDescriptorAllocator::GetLayout: immutable samplers aren't supported");
			key.push_back({ binding.binding, (uint32_t)binding.descriptorType, binding.descriptorCount, (uint32_t)binding.stageFlags });
		}

		auto it = s_Descriptors->Layouts.find(key);
		if (it != s_Descriptors->Layouts.end())
		{
			return it->second;
		}

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = (uint32_t)bindings.size();
		layoutInfo.pBindings = bindings.data();

		VkDescriptorSetLayout layout = VK_NULL_HANDLE;
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(Renderer::GetVulkanDevice().GetDevice(), &layoutInfo, nullptr, &layout));
		s_Descriptors->Layouts.emplace(std::move(key), layout);
		return layout;
	}

	DescriptorAllocation VulkanDescriptorAllocator::AllocatePersistent(VkDescriptorSetLayout layout)
	{
		std::vector<VkDescriptorPool>& pools = s_Descriptors->PersistentPools;

		// Freed sets leave room in the older pools; try them newest first, then make a new one.
		DescriptorAllocation allocation;
		for (auto it = pools.rbegin(); it != pools.rend(); ++it)
		{
			if (TryAllocate(*it, layout, allocation.Set))
			{
				allocation.Pool = *it;
				break;
			}
		}
		if (allocation.Set == VK_NULL_HANDLE)
		{
			pools.push_back(CreatePool(true));
			const bool allocated = TryAllocate(pools.back(), layout, allocation.Set);
			HY_CORE_ASSERT(allocated, "VulkanDescriptorAllocator: a set doesn't fit in an empty pool (more descriptors than SetsPerPool allows)");
			allocation.Pool = allocated ? pools.back() : VK_NULL_HANDLE;
		}

		if (allocation.Set != VK_NULL_HANDLE)
		{
			s_Descriptors->PersistentSets++;
		}
		return allocation;
	}

	void VulkanDescriptorAllocator::FreePersistent(const DescriptorAllocation& allocation)
	{
		if (allocation.Set == VK_NULL_HANDLE || s_Descriptors == nullptr)
		{
			return;
		}

		VkDevice device = Renderer::GetVulkanDevice().GetDevice();
		Renderer::SubmitResourceFree([device, allocation]()
		{
			vkFreeDescriptorSets(device, allocation.Pool, 1, &allocation.Set);
			if (s_Descriptors != nullptr)
			{
				s_Descriptors->PersistentSets--;
			}
		});
	}

	VkDescriptorSet VulkanDescriptorAllocator::AllocateFrame(VkDescriptorSetLayout layout)
	{
		const uint32_t frame = Renderer::GetRenderFrame();
		std::vector<VkDescriptorPool>& pools = s_Descriptors->FramePools[frame];
		size_t& cursor = s_Descriptors->FramePoolCursor[frame];

		VkDescriptorSet set = VK_NULL_HANDLE;
		while (cursor < pools.size())
		{
			if (TryAllocate(pools[cursor], layout, set))
			{
				return set;
			}
			cursor++;
		}

		pools.push_back(CreatePool(false));
		const bool allocated = TryAllocate(pools.back(), layout, set);
		HY_CORE_ASSERT(allocated, "VulkanDescriptorAllocator: a set doesn't fit in an empty pool (more descriptors than SetsPerPool allows)");
		return allocated ? set : VK_NULL_HANDLE;
	}

	void VulkanDescriptorAllocator::ResetFrame(uint32_t frameIndex)
	{
		if (s_Descriptors == nullptr)
		{
			return;
		}

		// The frame's fence has signalled: nothing still uses the sets this slot handed out last time.
		VkDevice device = Renderer::GetVulkanDevice().GetDevice();
		for (VkDescriptorPool pool : s_Descriptors->FramePools[frameIndex])
		{
			VK_CHECK_RESULT(vkResetDescriptorPool(device, pool, 0));
		}
		s_Descriptors->FramePoolCursor[frameIndex] = 0;
	}
}
