#include "hypch.h"
#include "VulkanPipeline.h"

#include "VulkanRendererContext.h"
#include "VulkanUtils.h"
#include "Hydro/Renderer/Renderer.h"

namespace Hydro
{
	static VkFormat ShaderDataTypeToVulkanFormat(ShaderDataType type)
	{
		switch (type)
		{
		case ShaderDataType::Float:     return VK_FORMAT_R32_SFLOAT;
		case ShaderDataType::Float2:    return VK_FORMAT_R32G32_SFLOAT;
		case ShaderDataType::Float3:    return VK_FORMAT_R32G32B32_SFLOAT;
		case ShaderDataType::Float4:    return VK_FORMAT_R32G32B32A32_SFLOAT;
		case ShaderDataType::Int:       return VK_FORMAT_R32_SINT;
		case ShaderDataType::Int2:      return VK_FORMAT_R32G32_SINT;
		case ShaderDataType::Int3:      return VK_FORMAT_R32G32B32_SINT;
		case ShaderDataType::Int4:      return VK_FORMAT_R32G32B32A32_SINT;
		case ShaderDataType::Bool:      return VK_FORMAT_R8_UINT;
		case ShaderDataType::Mat3:      // A matrix takes one attribute location per column: not supported yet.
		case ShaderDataType::Mat4:
		case ShaderDataType::None:      break;
		}

		HY_CORE_WARN("Vertex attribute type {} has no Vulkan format; using VK_FORMAT_UNDEFINED.", (int)type);
		return VK_FORMAT_UNDEFINED;
	}

	static VkPrimitiveTopology PrimitiveTopologyToVulkan(PrimitiveTopology topology)
	{
		switch (topology)
		{
		case PrimitiveTopology::Points:     return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		case PrimitiveTopology::Lines:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		case PrimitiveTopology::Triangles:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		case PrimitiveTopology::None:       break;
		}

		HY_CORE_WARN("PipelineSpecification::Topology is None; using a triangle list.");
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	}

	static VkPipelineColorBlendAttachmentState BlendModeToVulkan(BlendMode mode)
	{
		VkPipelineColorBlendAttachmentState state{};
		state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		switch (mode)
		{
		case BlendMode::None:
			state.blendEnable = VK_FALSE;
			break;
		case BlendMode::Alpha:
			state.blendEnable = VK_TRUE;
			state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			state.colorBlendOp = VK_BLEND_OP_ADD;
			state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			state.alphaBlendOp = VK_BLEND_OP_ADD;
			break;
		case BlendMode::Additive:
			state.blendEnable = VK_TRUE;
			state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
			state.colorBlendOp = VK_BLEND_OP_ADD;
			state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			state.alphaBlendOp = VK_BLEND_OP_ADD;
			break;
		}

		return state;
	}

	VulkanPipeline::VulkanPipeline(const PipelineSpecification& spec)
	{
		m_pipelineSpecification = spec;

		if (spec.Shader == nullptr)
		{
			throw std::runtime_error("PipelineSpecification::Shader is null.");
		}

		VulkanDevice& vulkanDevice = Renderer::GetVulkanDevice();
		VkDevice device = vulkanDevice.GetDevice();
		VkRenderPass renderpass = spec.RenderPass != VK_NULL_HANDLE ? spec.RenderPass : Renderer::GetVulkanSwapChain().GetRenderPass();

		// Setup vertex data.
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		VkVertexInputBindingDescription vertexInputBinding = {};
		vertexInputBinding.binding = 0;
		vertexInputBinding.stride = (uint32_t)spec.Layout.GetStride();
		vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		// Inpute attribute bindings describe shader attribute locations and memory layouts
		std::vector<VkVertexInputAttributeDescription> vertexInputAttributs(spec.Layout.GetElementCount());

		uint32_t location = 0;
		for (auto element : spec.Layout)
		{
			vertexInputAttributs[location].binding = 0;
			vertexInputAttributs[location].location = location;
			vertexInputAttributs[location].format = ShaderDataTypeToVulkanFormat(element.Type);
			vertexInputAttributs[location].offset = (uint32_t)element.Offset;

			location++;
		}

		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)vertexInputAttributs.size();
		vertexInputInfo.pVertexBindingDescriptions = &vertexInputBinding;
		vertexInputInfo.pVertexAttributeDescriptions = vertexInputAttributs.data();


		// End setup vertex data.

		std::vector<VkDynamicState> dynamicStates =
		{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = PrimitiveTopologyToVulkan(spec.Topology);
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		// Viewport and scissor are dynamic state, set in Bind() from the current target size.
		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;

		// Rasterizer
		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		if (spec.Wireframe)
		{
			if (vulkanDevice.IsFillModeNonSolidEnabled())
			{
				rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
			}
			else
			{
				HY_CORE_WARN("Wireframe pipelines need the fillModeNonSolid GPU feature; drawing filled.");
			}
		}

		rasterizer.lineWidth = 1.0f;
		if (spec.LineWidth != 1.0f)
		{
			if (vulkanDevice.IsWideLinesEnabled())
			{
				rasterizer.lineWidth = spec.LineWidth;
			}
			else
			{
				HY_CORE_WARN("LineWidth {0} needs the wideLines GPU feature; using 1.", spec.LineWidth);
			}
		}

		rasterizer.cullMode = spec.BackfaceCulling ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
		// Geometry is counter-clockwise in world space. Camera projections flip Y for Vulkan (Camera.h),
		// which keeps that winding counter-clockwise on screen.
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		std::vector<VkDescriptorSetLayout> setLayouts;
		setLayouts.reserve(spec.DescriptorSets.size());
		for (VulkanDescriptorBuilder* descriptorSet : spec.DescriptorSets)
		{
			if (descriptorSet == nullptr)
			{
				throw std::runtime_error("PipelineSpecification::DescriptorSets contains a null entry.");
			}

			setLayouts.push_back(descriptorSet->GetDescriptorSetLayout());
		}

		pipelineLayoutInfo.setLayoutCount = (uint32_t)setLayouts.size();
		pipelineLayoutInfo.pSetLayouts = setLayouts.empty() ? nullptr : setLayouts.data();

		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		// Ignored by Vulkan when the render pass has no depth attachment.
		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = spec.DepthTest ? VK_TRUE : VK_FALSE;
		depthStencil.depthWriteEnable = spec.DepthWrite ? VK_TRUE : VK_FALSE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.stencilTestEnable = VK_FALSE;

		VkPipelineColorBlendAttachmentState colorBlendAttachment = BlendModeToVulkan(spec.Blend);

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;

		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout));

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = spec.Shader->GetShaderStageInformation().data();
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = m_PipelineLayout;
		pipelineInfo.renderPass = renderpass;
		pipelineInfo.subpass = 0;
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
		pipelineInfo.basePipelineIndex = -1; // Optional

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_GraphicsPipeline));

		// The shader is only needed above.
		m_pipelineSpecification.Shader = nullptr;
	}

	VulkanPipeline::~VulkanPipeline()
	{
		if (m_GraphicsPipeline == VK_NULL_HANDLE && m_PipelineLayout == VK_NULL_HANDLE)
		{
			return;
		}

		// Frames in flight may still have it bound: free it once they are done.
		VkDevice device = Renderer::GetVulkanDevice().GetDevice();
		Renderer::SubmitResourceFree([device, pipeline = m_GraphicsPipeline, layout = m_PipelineLayout]()
		{
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, layout, nullptr);
		});
	}

	void VulkanPipeline::Bind(VkCommandBuffer commandBuffer, VkExtent2D extent)
	{
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)extent.width;
		viewport.height = (float)extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = extent;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	}

	void VulkanPipeline::BindDescriptorSets(VkCommandBuffer commandBuffer)
	{
		if (m_pipelineSpecification.DescriptorSets.empty())
		{
			return;
		}

		uint32_t currentFrame = Renderer::GetRenderFrame();

		std::vector<VkDescriptorSet> descriptorSets;
		descriptorSets.reserve(m_pipelineSpecification.DescriptorSets.size());
		for (VulkanDescriptorBuilder* descriptorSet : m_pipelineSpecification.DescriptorSets)
		{
			descriptorSets.push_back(descriptorSet->GetDescriptorSet(currentFrame));
		}

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	}

	void VulkanPipeline::BindDescriptorSet(VkCommandBuffer commandBuffer, uint32_t setIndex, VulkanDescriptorBuilder& descriptorSet)
	{
		VkDescriptorSet set = descriptorSet.GetDescriptorSet(Renderer::GetRenderFrame());
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, setIndex, 1, &set, 0, nullptr);
	}
}
