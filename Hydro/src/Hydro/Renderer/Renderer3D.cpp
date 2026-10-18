#include "hypch.h"
#include <unordered_map>

#include "Renderer3D.h"
#include "Hydro/Renderer/Renderer.h"
#include "Hydro/Platform/Vulkan/VulkanPipeline.h"
#include "Hydro/Platform/Vulkan/VulkanIndexBuffer.h"
#include "Hydro/Platform/Vulkan/VulkanUniformBuffer.h"
#include "Hydro/Renderer/Camera.h"
#include "Hydro/Platform/Vulkan/VullkanTexture.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

struct Vertex
{
	glm::vec3 Position;
	glm::vec4 Color;
	glm::vec2 texCoord;
	float textureIndex;

	bool operator==(const Vertex& other) const {
		return Position == other.Position && Color == other.Color && texCoord == other.texCoord && textureIndex == other.textureIndex;
	}
};

namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.Position) ^
				(hash<glm::vec3>()(vertex.Color) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.texCoord) << 1);
		}
	};
}

namespace Hydro
{
	struct UniformBufferObject
	{
		alignas(16) glm::mat4 mvp;
	};
	
	struct Mesh
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		UniformBufferObject ubo;

		Ref<VulkanShader> Shader;

		Ref<VulkanPipeline> QuadPipeline;
		Ref<VulkanVertexBuffer> QuadVertexBuffer;
		Ref<VulkanIndexBuffer> QuadIndexBuffer;
		Ref<VulkanUniformBuffer> QuadUniformBuffer;
		Ref<VulkanDescriptorBuilder> QuadDescriptorSet;

		Ref<VullkanTexture> VulkanTexture;
	};


	static Mesh* s_Data = nullptr;


	void Renderer3D::Init()
	{
		return;

		{
			/// Should be done before even entering the renderer shaders should have an get method from some sort of library.
			ShaderInformation vertexInformation;
			vertexInformation.path = "assets/shaders/vertex.vert";
			vertexInformation.shaderStage = VK_SHADER_STAGE_VERTEX_BIT;
			vertexInformation.shaderType = ShaderType::Vertex;

			ShaderInformation fragmentInformation;
			fragmentInformation.path = "assets/shaders/fragment.frag";
			fragmentInformation.shaderStage = VK_SHADER_STAGE_FRAGMENT_BIT;
			fragmentInformation.shaderType = ShaderType::Fragment;

			ShaderSpecification shaderSpecification;
			shaderSpecification.shaderInformation =
			{
				vertexInformation,
				fragmentInformation,
			};

			s_Data->Shader = CreateRef<VulkanShader>(shaderSpecification);
		}

		{
			VulkanTextureSpecification textureProperties;

			s_Data->VulkanTexture = CreateRef<VullkanTexture>("assets/textures/viking_room.png", textureProperties);

			s_Data->QuadVertexBuffer = CreateRef<VulkanVertexBuffer>((void*)s_Data->vertices.data(), s_Data->vertices.size() * sizeof(s_Data->vertices[0]));
			s_Data->QuadIndexBuffer = CreateRef<VulkanIndexBuffer>((void*)s_Data->indices.data(), s_Data->indices.size() * sizeof(s_Data->indices[0]));
			s_Data->QuadUniformBuffer = CreateRef<VulkanUniformBuffer>((uint32_t)sizeof(UniformBufferObject));

			s_Data->QuadDescriptorSet = CreateRef<VulkanDescriptorBuilder>();
			s_Data->QuadDescriptorSet->Begin();
			s_Data->QuadDescriptorSet->BindBuffer(0, s_Data->QuadUniformBuffer, sizeof(UniformBufferObject), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
			s_Data->QuadDescriptorSet->BindImage(1, s_Data->VulkanTexture, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
			s_Data->QuadDescriptorSet->Build();

			PipelineSpecification specification;
			specification.Layout =
			{
				{ "inPosition", ShaderDataType::Float3 },
				{ "inColor", ShaderDataType::Float4 },
				{ "inTexCoord", ShaderDataType::Float2 },
				{ "inTexIndex", ShaderDataType::Float },
			};

			specification.DescriptorSets = { s_Data->QuadDescriptorSet };
			specification.shader = s_Data->Shader;
			// Enable once the mesh winding is checked: the front face is counter-clockwise (see Camera.h).
			specification.BackfaceCulling = false;

			s_Data->QuadPipeline = CreateRef<VulkanPipeline>(specification);
		}
	}
	
	void Renderer3D::Shutdown()
	{
		if (s_Data == nullptr)
		{
			return;
		}

		s_Data->QuadDescriptorSet->Destroy();
		s_Data->VulkanTexture->Destroy();
		s_Data->QuadPipeline->Destroy();
		s_Data->QuadUniformBuffer->Destroy();
		s_Data->QuadIndexBuffer->Destroy();
		s_Data->QuadVertexBuffer->Destroy();

		delete s_Data;
		s_Data = nullptr;
	}
	
	void Renderer3D::Begin()
	{
		uint32_t currentImage = Renderer::GetRenderFrame();
		VkExtent2D extent = Renderer::GetVulkanSwapChain()->GetExtent();

		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		glm::mat4 model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		glm::mat4 view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		// Camera::Perspective handles the Vulkan depth range and Y flip.
		glm::mat4 proj = Camera::Perspective(glm::radians(45.0f), extent.width / (float)extent.height, 0.1f, 10.0f);

		s_Data->ubo.mvp = proj * view * model;
		s_Data->QuadUniformBuffer->Update(&s_Data->ubo.mvp, currentImage, (uint32_t)sizeof(UniformBufferObject));
	}
	
	void Renderer3D::End()
	{
	}
	
	void Renderer3D::DrawMesh()
	{
		// Must be called while the swapchain render pass is active (this pipeline targets it).
		VkCommandBuffer commandBuffer = Renderer::GetCurrentCommandBuffer();

		s_Data->QuadPipeline->Bind(commandBuffer, Renderer::GetVulkanSwapChain()->GetExtent());
		s_Data->QuadVertexBuffer->Bind(commandBuffer);
		s_Data->QuadIndexBuffer->Bind(commandBuffer);
		s_Data->QuadPipeline->BindDescriptorSets(commandBuffer);

		vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(s_Data->indices.size()), 1, 0, 0, 0);
	}

}
