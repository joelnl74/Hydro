#include "hypch.h"
#include "PassTimers.h"

#include "Hydro/Platform/Vulkan/VulkanFrameContext.h"
#include "Hydro/Platform/Vulkan/VulkanUtils.h"
#include "Hydro/Renderer/Renderer.h"

#include <cstring>

namespace Hydro
{
	struct PassTimersData
	{
		// Per frame in flight: queries 0 and 1 time all views, then each of the first MaxTimedViews lit views has a
		// begin/end pair per TimerPhase.
		static constexpr uint32_t MaxTimedViews = 4;
		static constexpr uint32_t TimerPhaseCount = 3;
		static constexpr uint32_t QueriesPerFrame = 2 + MaxTimedViews * TimerPhaseCount * 2;
		VkQueryPool TimestampPool = VK_NULL_HANDLE;
		float TimestampPeriodNs = 0.0f;
		std::array<bool, MAX_FRAMES_IN_FLIGHT> TimestampsWritten{};
		float LastGPUTimeMs = -1.0f;
		float LastLightPassGPUMs = -1.0f;
		float LastShadowGPUMs = -1.0f;
		float LastCompositeGPUMs = -1.0f;
		// Which phases of which timed view each frame slot wrote (a bit per phase).
		std::array<std::array<uint32_t, MaxTimedViews>, MAX_FRAMES_IN_FLIGHT> TimedPhaseMask{};
		uint32_t TimedViewCount = 0;
		uint32_t CurrentTimerSlot = UINT32_MAX;

		// A second pool for the render graph passes; their names are summed over the views.
		static constexpr uint32_t MaxTimedPasses = 48;
		VkQueryPool PassTimestampPool = VK_NULL_HANDLE;
		std::array<std::vector<std::string>, MAX_FRAMES_IN_FLIGHT> TimedPassNames;
		std::vector<PassTiming> LastPassTimings;

		// Null when the instance doesn't have VK_EXT_debug_utils.
		PFN_vkCmdBeginDebugUtilsLabelEXT BeginLabel = nullptr;
		PFN_vkCmdEndDebugUtilsLabelEXT EndLabel = nullptr;

		char GPUName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE] = {};
	};

	static PassTimersData* s_Data = nullptr;

	void PassTimers::Init()
	{
		HY_CORE_ASSERT(s_Data == nullptr, "PassTimers::Init called twice");
		s_Data = new PassTimersData();

		// An instance extension: its commands come from the instance.
		VkInstance instance = Renderer::GetRendererContext().GetInstance();
		s_Data->BeginLabel = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT");
		s_Data->EndLabel = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT");
		if (s_Data->BeginLabel == nullptr || s_Data->EndLabel == nullptr)
		{
			s_Data->BeginLabel = nullptr;
			s_Data->EndLabel = nullptr;
		}

		VulkanDevice& device = Renderer::GetVulkanDevice();
		const VkPhysicalDeviceProperties& properties = device.GetPhysicalDevice().GetProperties();
		memcpy(s_Data->GPUName, properties.deviceName, sizeof(s_Data->GPUName));

		if (properties.limits.timestampComputeAndGraphics)
		{
			s_Data->TimestampPeriodNs = properties.limits.timestampPeriod;

			VkQueryPoolCreateInfo queryPoolInfo{};
			queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
			queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
			queryPoolInfo.queryCount = PassTimersData::QueriesPerFrame * MAX_FRAMES_IN_FLIGHT;
			VK_CHECK_RESULT(vkCreateQueryPool(device.GetDevice(), &queryPoolInfo, nullptr, &s_Data->TimestampPool));

			queryPoolInfo.queryCount = PassTimersData::MaxTimedPasses * 2 * MAX_FRAMES_IN_FLIGHT;
			VK_CHECK_RESULT(vkCreateQueryPool(device.GetDevice(), &queryPoolInfo, nullptr, &s_Data->PassTimestampPool));
		}
	}

	void PassTimers::Shutdown()
	{
		if (s_Data == nullptr)
		{
			return;
		}

		VkDevice device = Renderer::GetVulkanDevice().GetDevice();
		if (s_Data->TimestampPool != VK_NULL_HANDLE)
		{
			Renderer::SubmitResourceFree([device, pool = s_Data->TimestampPool]()
			{
				vkDestroyQueryPool(device, pool, nullptr);
			});
		}
		if (s_Data->PassTimestampPool != VK_NULL_HANDLE)
		{
			Renderer::SubmitResourceFree([device, pool = s_Data->PassTimestampPool]()
			{
				vkDestroyQueryPool(device, pool, nullptr);
			});
		}

		delete s_Data;
		s_Data = nullptr;
	}

	// Sums the pass timestamps `frame` wrote last time by pass name, then resets its queries.
	static void ReadPassTimings(VkCommandBuffer commandBuffer, uint32_t frame)
	{
		std::vector<std::string>& names = s_Data->TimedPassNames[frame];
		if (names.empty() == false)
		{
			std::vector<uint64_t> results(names.size() * 4, 0);
			const VkResult result = vkGetQueryPoolResults(Renderer::GetVulkanDevice().GetDevice(), s_Data->PassTimestampPool,
				frame * PassTimersData::MaxTimedPasses * 2, (uint32_t)names.size() * 2, results.size() * sizeof(uint64_t), results.data(),
				2 * sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
			if (result == VK_SUCCESS || result == VK_NOT_READY)
			{
				const double toMs = (double)s_Data->TimestampPeriodNs / 1.0e6;
				std::vector<PassTiming> timings;
				for (size_t i = 0; i < names.size(); i++)
				{
					const uint64_t begin = results[i * 4], beginReady = results[i * 4 + 1];
					const uint64_t end = results[i * 4 + 2], endReady = results[i * 4 + 3];
					if (beginReady == 0 || endReady == 0 || end < begin)
					{
						continue;
					}
					const float ms = (float)((double)(end - begin) * toMs);
					auto found = std::find_if(timings.begin(), timings.end(),
						[&](const PassTiming& timing) { return timing.Name == names[i]; });
					if (found == timings.end())
					{
						timings.push_back({ names[i], ms });
					}
					else
					{
						found->GPUMs += ms;
					}
				}
				s_Data->LastPassTimings = std::move(timings);
			}
		}
		names.clear();
		vkCmdResetQueryPool(commandBuffer, s_Data->PassTimestampPool, frame * PassTimersData::MaxTimedPasses * 2,
			PassTimersData::MaxTimedPasses * 2);
	}

	// The frame and lighting-phase timestamps `frame` wrote last time.
	static void ReadFrameTimings(uint32_t frame, uint32_t firstQuery)
	{
		// {value, availability} per query. Queries that weren't written stay unavailable (VK_NOT_READY) and are skipped.
		uint64_t results[PassTimersData::QueriesPerFrame * 2] = {};
		VkResult result = vkGetQueryPoolResults(Renderer::GetVulkanDevice().GetDevice(), s_Data->TimestampPool, firstQuery,
			PassTimersData::QueriesPerFrame, sizeof(results), results, 2 * sizeof(uint64_t),
			VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
		if (result != VK_SUCCESS && result != VK_NOT_READY)
		{
			return;
		}

		const double toMs = (double)s_Data->TimestampPeriodNs / 1.0e6;
		if (results[1] != 0 && results[3] != 0 && results[2] >= results[0])
		{
			s_Data->LastGPUTimeMs = (float)((double)(results[2] - results[0]) * toMs);
		}

		double phaseMs[PassTimersData::TimerPhaseCount] = {};
		bool anyLit = false;
		for (uint32_t slot = 0; slot < PassTimersData::MaxTimedViews; slot++)
		{
			for (uint32_t phase = 0; phase < PassTimersData::TimerPhaseCount; phase++)
			{
				if ((s_Data->TimedPhaseMask[frame][slot] & (1u << phase)) == 0)
				{
					continue;
				}

				const uint32_t query = 2 + (slot * PassTimersData::TimerPhaseCount + phase) * 2;
				const bool available = results[query * 2 + 1] != 0 && results[(query + 1) * 2 + 1] != 0;
				if (available && results[(query + 1) * 2] >= results[query * 2])
				{
					phaseMs[phase] += (double)(results[(query + 1) * 2] - results[query * 2]) * toMs;
					anyLit = anyLit || phase == 0;
				}
			}
		}

		s_Data->LastLightPassGPUMs = anyLit ? (float)phaseMs[0] : -1.0f;
		s_Data->LastShadowGPUMs = anyLit ? (float)phaseMs[1] : -1.0f;
		s_Data->LastCompositeGPUMs = anyLit ? (float)phaseMs[2] : -1.0f;
	}

	void PassTimers::BeginFrame(VkCommandBuffer commandBuffer, uint32_t frame)
	{
		if (s_Data->PassTimestampPool != VK_NULL_HANDLE)
		{
			ReadPassTimings(commandBuffer, frame);
		}

		const uint32_t firstQuery = frame * PassTimersData::QueriesPerFrame;
		s_Data->TimedViewCount = 0;
		s_Data->CurrentTimerSlot = UINT32_MAX;
		if (s_Data->TimestampPool == VK_NULL_HANDLE)
		{
			return;
		}

		if (s_Data->TimestampsWritten[frame])
		{
			ReadFrameTimings(frame, firstQuery);
		}
		s_Data->TimedPhaseMask[frame] = {};
		vkCmdResetQueryPool(commandBuffer, s_Data->TimestampPool, firstQuery, PassTimersData::QueriesPerFrame);
		vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, s_Data->TimestampPool, firstQuery);
	}

	void PassTimers::EndFrame(VkCommandBuffer commandBuffer, uint32_t frame)
	{
		if (s_Data->TimestampPool == VK_NULL_HANDLE)
		{
			return;
		}
		vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, s_Data->TimestampPool,
			frame * PassTimersData::QueriesPerFrame + 1);
		s_Data->TimestampsWritten[frame] = true;
	}

	void PassTimers::FillStats(Statistics& stats)
	{
		stats.GPUTimeMs = s_Data->LastGPUTimeMs;
		stats.LightPassGPUMs = s_Data->LastLightPassGPUMs;
		stats.ShadowGPUMs = s_Data->LastShadowGPUMs;
		stats.CompositeGPUMs = s_Data->LastCompositeGPUMs;
		stats.GPUName = s_Data->GPUName;
	}

	void PassTimers::BeginLabel(VkCommandBuffer commandBuffer, const char* name)
	{
		if (s_Data->BeginLabel == nullptr)
		{
			return;
		}
		VkDebugUtilsLabelEXT label{};
		label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		label.pLabelName = name;
		s_Data->BeginLabel(commandBuffer, &label);
	}

	void PassTimers::EndLabel(VkCommandBuffer commandBuffer)
	{
		if (s_Data->EndLabel != nullptr)
		{
			s_Data->EndLabel(commandBuffer);
		}
	}

	uint32_t PassTimers::BeginPass(VkCommandBuffer commandBuffer, uint32_t frame, const std::string& name)
	{
		std::vector<std::string>& names = s_Data->TimedPassNames[frame];
		if (s_Data->PassTimestampPool == VK_NULL_HANDLE || names.size() >= PassTimersData::MaxTimedPasses)
		{
			return UINT32_MAX;
		}

		const uint32_t timer = (uint32_t)names.size();
		names.push_back(name);
		const uint32_t query = (frame * PassTimersData::MaxTimedPasses + timer) * 2;
		vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, s_Data->PassTimestampPool, query);
		return timer;
	}

	void PassTimers::EndPass(VkCommandBuffer commandBuffer, uint32_t frame, uint32_t timer)
	{
		if (timer == UINT32_MAX)
		{
			return;
		}
		const uint32_t query = (frame * PassTimersData::MaxTimedPasses + timer) * 2 + 1;
		vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, s_Data->PassTimestampPool, query);
	}

	const std::vector<PassTiming>& PassTimers::GetPassTimings()
	{
		return s_Data->LastPassTimings;
	}

	void PassTimers::BeginLitView()
	{
		s_Data->CurrentTimerSlot = s_Data->TimedViewCount++;
	}

	void PassTimers::WritePhase(VkCommandBuffer commandBuffer, uint32_t frame, TimerPhase phase, bool end)
	{
		const uint32_t slot = s_Data->CurrentTimerSlot;
		if (s_Data->TimestampPool == VK_NULL_HANDLE || slot >= PassTimersData::MaxTimedViews)
		{
			return;
		}

		const uint32_t phaseIndex = (uint32_t)phase;
		const uint32_t pair = slot * PassTimersData::TimerPhaseCount + phaseIndex;
		const uint32_t query = frame * PassTimersData::QueriesPerFrame + 2 + pair * 2 + (end ? 1u : 0u);
		vkCmdWriteTimestamp(commandBuffer, end ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			s_Data->TimestampPool, query);
		if (end)
		{
			s_Data->TimedPhaseMask[frame][slot] |= 1u << phaseIndex;
		}
	}
}
