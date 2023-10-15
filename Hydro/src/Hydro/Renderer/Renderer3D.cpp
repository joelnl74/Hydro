#include <hypch.h>
#include <unordered_map>

#include "Renderer3D.h"
#include "Hydro/Renderer/Renderer.h"
#include "Hydro/Platform/Vulkan/VulkanPipeline.h"
#include "Hydro/Platform/Vulkan/VulkanIndexBuffer.h"
#include "Hydro/Platform/Vulkan/VulkanUniformBuffer.h"
#include "Hydro/Platform/Vulkan/VullkanTexture.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "Vendor/Obj-loader/tiny_obj_loader.h"

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

	bool operator==(const Vertex& other) const {
		return Position == other.Position && Color == other.Color && texCoord == other.texCoord;
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
		s_Data = new Mesh();

		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn, err;

		std::unordered_map<Vertex, uint32_t> uniqueVertices{};

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "assets/meshes/viking.obj")) {
			throw std::runtime_error(warn + err);
		}

		for (const auto& shape : shapes) {
			for (const auto& index : shape.mesh.indices) {
				Vertex vertex{};

				vertex.Position = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};

				vertex.texCoord = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
				};

				vertex.Color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

				if (uniqueVertices.count(vertex) == 0) {
					uniqueVertices[vertex] = static_cast<uint32_t>(s_Data->vertices.size());
					s_Data->vertices.push_back(vertex);
				}

				s_Data->indices.push_back(uniqueVertices[vertex]);
			}
		}

		{
			/// Should be done before even entering the renderer shaders should have an get method from some sort of library.
			ShaderInformation vertexInformation;
			vertexInformation.path = "assets/shaders/vertex.spv";
			vertexInformation.shaderStage = VK_SHADER_STAGE_VERTEX_BIT;
			vertexInformation.shaderType = ShaderType::Vertex;

			ShaderInformation fragmentInformation;
			fragmentInformation.path = "assets/shaders/fragment.spv";
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
			s_Data->QuadUniformBuffer = CreateRef<VulkanUniformBuffer>((uint8_t)sizeof(UniformBufferObject));

			s_Data->QuadDescriptorSet = CreateRef<VulkanDescriptorBuilder>();
			s_Data->QuadDescriptorSet->Begin();
			s_Data->QuadDescriptorSet->BindBuffer(0, s_Data->QuadUniformBuffer, sizeof(UniformBufferObject), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
			s_Data->QuadDescriptorSet->BindImage(1, s_Data->VulkanTexture, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
			s_Data->QuadDescriptorSet->Build();

			PipelineSpecification specification;
			specification.Layout =
			{
				{ ShaderDataType::Float3, "inPosition" },
				{ ShaderDataType::Float4, "inColor" },
				{ ShaderDataType::Float2, "inTexCoord" },
			};

			specification.descriptorSet = s_Data->QuadDescriptorSet;
			specification.shader = s_Data->Shader;

			s_Data->QuadPipeline = CreateRef<VulkanPipeline>(specification);
		}
	}
	
	void Renderer3D::ShutDown()
	{
		s_Data->QuadDescriptorSet->Destory();
		s_Data->VulkanTexture->Destory();
		s_Data->Shader->Destory();
		s_Data->QuadPipeline->ShutDown();
		s_Data->QuadUniformBuffer->Destory();
		s_Data->QuadIndexBuffer->Destory();
		s_Data->QuadVertexBuffer->Destory();

		s_Data = nullptr;
	}
	
	void Renderer3D::Begin()
	{
		uint32_t currentImage = Renderer::GetRenderFrame();
		auto& extent = Renderer::GetVulkanSwapChain()->GetExtend();

		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		glm::mat4 model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		glm::mat4 view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		glm::mat4 proj = glm::perspective(glm::radians(45.0f), extent.width / (float)extent.height, 0.1f, 10.0f);
		proj[1][1] *= -1;

		s_Data->ubo.mvp = proj * view * model;
		s_Data->QuadUniformBuffer->Update(&s_Data->ubo.mvp, currentImage, (uint8_t)sizeof(UniformBufferObject));
	}
	
	void Renderer3D::End()
	{
	}
	
	void Renderer3D::DrawMesh()
	{
		auto commandBuffer = Renderer::GetVulkanSwapChain()->GetCommandBuffer();

		s_Data->QuadPipeline->Bind();
		s_Data->QuadVertexBuffer->Bind();
		s_Data->QuadIndexBuffer->Bind();
		s_Data->QuadPipeline->BindDescriptorSets();

		vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(s_Data->indices.size()), 1, 0, 0, 0);
	}

}
