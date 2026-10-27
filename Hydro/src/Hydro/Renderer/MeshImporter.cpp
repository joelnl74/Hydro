#include "hypch.h"
#include "MeshImporter.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <set>
#include <utility>

namespace Hydro
{
	static std::string ToName(const char* name, const std::string& fallback)
	{
		return name != nullptr && name[0] != '\0' ? std::string(name) : fallback;
	}

	static const char* ResultToString(cgltf_result result)
	{
		switch (result)
		{
			case cgltf_result_success: return "success";
			case cgltf_result_data_too_short: return "data too short";
			case cgltf_result_unknown_format: return "not a glTF file";
			case cgltf_result_invalid_json: return "invalid JSON";
			case cgltf_result_invalid_gltf: return "invalid glTF";
			case cgltf_result_invalid_options: return "invalid options";
			case cgltf_result_file_not_found: return "a file it refers to was not found";
			case cgltf_result_io_error: return "I/O error";
			case cgltf_result_out_of_memory: return "out of memory";
			case cgltf_result_legacy_gltf: return "glTF 1.0 (only 2.0 is supported)";
			default: return "error";
		}
	}

	// Index of `item` in `base`, or -1 for nullptr.
	template<typename T>
	static int32_t IndexOf(const T* item, const T* base)
	{
		return item == nullptr ? -1 : (int32_t)(item - base);
	}

	// The image a texture view shows, or -1. Formats stb_image can't decode (KTX2 / basisu, WebP) leave
	// the texture without an image: reported once per texture.
	static int32_t ReadTextureImage(const cgltf_texture_view& view, const cgltf_data* data, const std::filesystem::path& path, std::set<const cgltf_texture*>& reported)
	{
		if (view.texture == nullptr)
		{
			return -1;
		}
		if (view.texture->image == nullptr)
		{
			if (reported.insert(view.texture).second)
			{
				HY_CORE_WARN("MeshImporter: '{}': texture '{}' uses an image format the engine can't decode (KTX2 / WebP); its material uses its colour",
					path.filename().string(), ToName(view.texture->name, "?"));
			}
			return -1;
		}
		if (view.texcoord != 0)
		{
			HY_CORE_WARN("MeshImporter: '{}': a texture uses TEXCOORD_{}; only TEXCOORD_0 is imported", path.filename().string(), view.texcoord);
		}
		return IndexOf(view.texture->image, data->images);
	}

	bool MeshImporter::Import(const std::filesystem::path& path, ImportedModel& model)
	{
		model = {};

		cgltf_options options{};
		cgltf_data* data = nullptr;
		const std::string file = path.string();
		cgltf_result result = cgltf_parse_file(&options, file.c_str(), &data);
		if (result != cgltf_result_success)
		{
			HY_CORE_ERROR("MeshImporter: can't read '{}': {}", file, ResultToString(result));
			return false;
		}
		result = cgltf_load_buffers(&options, data, file.c_str());
		if (result != cgltf_result_success)
		{
			HY_CORE_ERROR("MeshImporter: can't load the buffers of '{}': {}", file, ResultToString(result));
			cgltf_free(data);
			return false;
		}

		const std::string fileName = path.filename().string();
		if (data->skins_count > 0 || data->animations_count > 0)
		{
			HY_CORE_INFO("MeshImporter: '{}': {} skins and {} animations ignored (static meshes only)", fileName, data->skins_count, data->animations_count);
		}

		// Images.
		model.Images.resize(data->images_count);
		for (cgltf_size i = 0; i < data->images_count; i++)
		{
			const cgltf_image& source = data->images[i];
			ImportedImage& image = model.Images[i];
			image.Name = ToName(source.name, "image" + std::to_string(i));
			image.MimeType = source.mime_type != nullptr ? source.mime_type : "";

			if (source.buffer_view != nullptr)
			{
				const uint8_t* bytes = cgltf_buffer_view_data(source.buffer_view);
				if (bytes != nullptr)
				{
					image.EmbeddedData.assign(bytes, bytes + source.buffer_view->size);
				}
			}
			else if (source.uri != nullptr && std::strncmp(source.uri, "data:", 5) == 0)
			{
				// data:image/png;base64,....
				const char* comma = std::strchr(source.uri, ',');
				const char* semicolon = std::strchr(source.uri, ';');
				if (comma != nullptr && semicolon != nullptr && semicolon < comma && std::strncmp(semicolon, ";base64", 7) == 0)
				{
					image.MimeType = std::string((const char*)source.uri + 5, semicolon);
					const char* base64 = comma + 1;
					const size_t length = std::strlen(base64);
					size_t padding = 0;
					if (length > 0 && base64[length - 1] == '=')
					{
						padding++;
					}
					if (length > 1 && base64[length - 2] == '=')
					{
						padding++;
					}
					const cgltf_size size = length / 4 * 3 - padding;
					void* decoded = nullptr;
					if (cgltf_load_buffer_base64(&options, size, base64, &decoded) == cgltf_result_success)
					{
						const uint8_t* bytes = (const uint8_t*)decoded;
						image.EmbeddedData.assign(bytes, bytes + size);
						std::free(decoded);
					}
				}
				if (image.EmbeddedData.empty())
				{
					HY_CORE_WARN("MeshImporter: '{}': image {} has a data URI that can't be read", fileName, image.Name);
				}
			}
			else if (source.uri != nullptr)
			{
				std::string uri = source.uri;
				cgltf_decode_uri(uri.data());
				uri.resize(std::strlen(uri.c_str()));
				image.File = (path.parent_path() / std::filesystem::u8path(uri)).lexically_normal();
			}
		}

		// Materials.
		std::set<const cgltf_texture*> reportedTextures;
		bool reportedBlend = false;
		bool reportedIgnoredMaps = false;
		model.Materials.resize(data->materials_count);
		for (cgltf_size i = 0; i < data->materials_count; i++)
		{
			const cgltf_material& source = data->materials[i];
			ImportedMaterial& material = model.Materials[i];
			material.Name = ToName(source.name, "Material" + std::to_string(i));

			if (source.has_pbr_metallic_roughness)
			{
				const cgltf_pbr_metallic_roughness& pbr = source.pbr_metallic_roughness;
				material.BaseColor = glm::make_vec4(pbr.base_color_factor);
				material.BaseColorImage = ReadTextureImage(pbr.base_color_texture, data, path, reportedTextures);
				if (pbr.base_color_texture.has_transform)
				{
					material.Tiling = glm::make_vec2(pbr.base_color_texture.transform.scale);
				}
				if (pbr.base_color_texture.texture != nullptr && pbr.base_color_texture.texture->sampler != nullptr)
				{
					material.LinearFilter = pbr.base_color_texture.texture->sampler->mag_filter == 9729; // GL_LINEAR
				}
			}
			else if (source.has_pbr_specular_glossiness)
			{
				material.BaseColor = glm::make_vec4(source.pbr_specular_glossiness.diffuse_factor);
				material.BaseColorImage = ReadTextureImage(source.pbr_specular_glossiness.diffuse_texture, data, path, reportedTextures);
			}

			material.Emissive = glm::make_vec3(source.emissive_factor);
			material.EmissiveImage = ReadTextureImage(source.emissive_texture, data, path, reportedTextures);
			if (source.has_emissive_strength)
			{
				material.EmissiveStrength = source.emissive_strength.emissive_strength;
			}

			if (source.alpha_mode == cgltf_alpha_mode_mask)
			{
				material.AlphaCutoff = source.alpha_cutoff;
			}
			else if (source.alpha_mode == cgltf_alpha_mode_blend)
			{
				// No translucent meshes in the MVP: cut out at half, which keeps leaves and railings.
				material.AlphaCutoff = 0.5f;
				if (reportedBlend == false)
				{
					HY_CORE_WARN("MeshImporter: '{}': BLEND materials are drawn cut out at alpha 0.5 (no translucent meshes yet)", fileName);
					reportedBlend = true;
				}
			}

			if (reportedIgnoredMaps == false && (source.normal_texture.texture != nullptr || source.occlusion_texture.texture != nullptr
				|| source.pbr_metallic_roughness.metallic_roughness_texture.texture != nullptr))
			{
				HY_CORE_INFO("MeshImporter: '{}': normal, occlusion and metallic-roughness maps are ignored (Standard material)", fileName);
				reportedIgnoredMaps = true;
			}
		}

		// Meshes.
		bool reportedMorphs = false;
		model.Meshes.resize(data->meshes_count);
		for (cgltf_size m = 0; m < data->meshes_count; m++)
		{
			const cgltf_mesh& source = data->meshes[m];
			ImportedMesh& mesh = model.Meshes[m];
			mesh.Name = ToName(source.name, "Mesh" + std::to_string(m));

			for (cgltf_size p = 0; p < source.primitives_count; p++)
			{
				const cgltf_primitive& primitiveSource = source.primitives[p];
				if (primitiveSource.type != cgltf_primitive_type_triangles)
				{
					HY_CORE_WARN("MeshImporter: '{}': mesh '{}' primitive {} isn't a triangle list; left out", fileName, mesh.Name, p);
					continue;
				}
				if (primitiveSource.has_draco_mesh_compression)
				{
					HY_CORE_WARN("MeshImporter: '{}': mesh '{}' uses Draco compression, which isn't supported; left out", fileName, mesh.Name);
					continue;
				}
				if (primitiveSource.targets_count > 0 && reportedMorphs == false)
				{
					HY_CORE_INFO("MeshImporter: '{}': morph targets ignored", fileName);
					reportedMorphs = true;
				}

				ImportedPrimitive primitive;
				primitive.Material = IndexOf(primitiveSource.material, data->materials);
				for (cgltf_size a = 0; a < primitiveSource.attributes_count; a++)
				{
					const cgltf_attribute& attribute = primitiveSource.attributes[a];
					const cgltf_accessor* accessor = attribute.data;
					if (attribute.index != 0 || accessor == nullptr)
					{
						continue;
					}

					const cgltf_size count = accessor->count;
					switch (attribute.type)
					{
						case cgltf_attribute_type_position:
							primitive.Positions.resize(count);
							for (cgltf_size v = 0; v < count; v++)
							{
								cgltf_accessor_read_float(accessor, v, glm::value_ptr(primitive.Positions[v]), 3);
							}
							break;
						case cgltf_attribute_type_normal:
							primitive.Normals.resize(count);
							for (cgltf_size v = 0; v < count; v++)
							{
								cgltf_accessor_read_float(accessor, v, glm::value_ptr(primitive.Normals[v]), 3);
							}
							break;
						case cgltf_attribute_type_texcoord:
							primitive.UVs.resize(count);
							for (cgltf_size v = 0; v < count; v++)
							{
								cgltf_accessor_read_float(accessor, v, glm::value_ptr(primitive.UVs[v]), 2);
							}
							break;
						case cgltf_attribute_type_color:
						{
							// RGB or RGBA, normalised integers or floats; cgltf converts.
							const cgltf_size components = cgltf_num_components(accessor->type);
							primitive.Colors.assign(count, glm::vec4(1.0f));
							for (cgltf_size v = 0; v < count; v++)
							{
								cgltf_accessor_read_float(accessor, v, glm::value_ptr(primitive.Colors[v]), components < 4 ? components : 4);
							}
							break;
						}
						default:
							break;
					}
				}

				if (primitive.Positions.empty())
				{
					HY_CORE_WARN("MeshImporter: '{}': mesh '{}' primitive {} has no positions; left out", fileName, mesh.Name, p);
					continue;
				}

				if (primitiveSource.indices != nullptr)
				{
					primitive.Indices.resize(primitiveSource.indices->count);
					for (cgltf_size i = 0; i < primitiveSource.indices->count; i++)
					{
						primitive.Indices[i] = (uint32_t)cgltf_accessor_read_index(primitiveSource.indices, i);
					}
				}
				else
				{
					primitive.Indices.resize(primitive.Positions.size());
					for (uint32_t i = 0; i < (uint32_t)primitive.Indices.size(); i++)
					{
						primitive.Indices[i] = i;
					}
				}
				primitive.Indices.resize(primitive.Indices.size() / 3 * 3);

				mesh.Primitives.push_back(std::move(primitive));
			}
		}

		// Nodes.
		model.Nodes.resize(data->nodes_count);
		for (cgltf_size n = 0; n < data->nodes_count; n++)
		{
			const cgltf_node& source = data->nodes[n];
			ImportedNode& node = model.Nodes[n];
			node.Name = ToName(source.name, "Node" + std::to_string(n));
			cgltf_node_transform_local(&source, glm::value_ptr(node.LocalTransform));
			node.Parent = IndexOf(source.parent, data->nodes);
			node.MeshIndex = IndexOf(source.mesh, data->meshes);
			for (cgltf_size c = 0; c < source.children_count; c++)
			{
				node.Children.push_back((uint32_t)IndexOf(source.children[c], data->nodes));
			}
		}

		// The selected scene: the file's `scene`, else the first; without scenes, every root node.
		const cgltf_scene* scene = data->scene != nullptr ? data->scene : (data->scenes_count > 0 ? &data->scenes[0] : nullptr);
		if (data->scenes_count > 1)
		{
			HY_CORE_INFO("MeshImporter: '{}' has {} scenes; only '{}' is imported", fileName, data->scenes_count, ToName(scene->name, "the first"));
		}
		if (scene != nullptr)
		{
			for (cgltf_size n = 0; n < scene->nodes_count; n++)
			{
				model.RootNodes.push_back((uint32_t)IndexOf(scene->nodes[n], data->nodes));
			}
		}
		else
		{
			for (uint32_t n = 0; n < (uint32_t)model.Nodes.size(); n++)
			{
				if (model.Nodes[n].Parent < 0)
				{
					model.RootNodes.push_back(n);
				}
			}
		}

		cgltf_free(data);
		return true;
	}

	glm::mat4 MeshImporter::GetImportTransform(const MeshImportConfig& config)
	{
		glm::mat4 transform = glm::scale(glm::mat4(1.0f), glm::vec3(config.Scale));
		if (config.ConvertYUpToZUp)
		{
			// (x, y, z) -> (x, -z, y): glTF's up (+Y) becomes +Z, its front (+Z) faces -Y.
			transform = transform * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		}
		return transform;
	}

	bool MeshImporter::GetCollisionKind(const std::string& nodeName, MeshCollisionKind& kind)
	{
		std::string name = nodeName;
		// Blender numbers copies "Name.001": the suffix comes before that.
		const size_t dot = name.find_last_of('.');
		if (dot != std::string::npos && dot + 1 < name.size()
			&& std::all_of(name.begin() + (std::ptrdiff_t)dot + 1, name.end(), [](char c) { return c >= '0' && c <= '9'; }))
		{
			name.resize(dot);
		}
		std::transform(name.begin(), name.end(), name.begin(), [](char c) { return (char)std::toupper((unsigned char)c); });

		// The kit's prefix: "COL_<object><suffix>", walkable when the suffix says so.
		if (name.size() > 4 && name.compare(0, 4, "COL_") == 0)
		{
			kind = name.find("_WALK", 3) != std::string::npos ? MeshCollisionKind::Walk : MeshCollisionKind::Convex;
			return true;
		}

		const auto endsWith = [&name](const std::string& suffix)
		{
			return name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
		};
		if (endsWith("_WALK"))
		{
			kind = MeshCollisionKind::Walk;
			return true;
		}
		if (endsWith("_COL"))
		{
			kind = MeshCollisionKind::Convex;
			return true;
		}
		return false;
	}

	MeshData MeshImporter::Flatten(const ImportedModel& model, const MeshImportConfig& config)
	{
		MeshData data;
		const glm::mat4 importTransform = GetImportTransform(config);
		const uint32_t defaultSlot = (uint32_t)model.Materials.size();

		// Depth first from the scene's roots, in file order; a node reached twice (a broken file) only once.
		std::vector<bool> visited(model.Nodes.size(), false);
		std::vector<std::pair<uint32_t, glm::mat4>> stack;
		for (auto it = model.RootNodes.rbegin(); it != model.RootNodes.rend(); ++it)
		{
			stack.push_back({ *it, importTransform });
		}

		while (stack.empty() == false)
		{
			const auto [nodeIndex, parentTransform] = stack.back();
			stack.pop_back();
			if (nodeIndex >= model.Nodes.size() || visited[nodeIndex])
			{
				continue;
			}
			visited[nodeIndex] = true;

			const ImportedNode& node = model.Nodes[nodeIndex];
			const glm::mat4 world = parentTransform * node.LocalTransform;
			for (auto it = node.Children.rbegin(); it != node.Children.rend(); ++it)
			{
				stack.push_back({ *it, world });
			}

			if (node.MeshIndex < 0 || node.MeshIndex >= (int32_t)model.Meshes.size())
			{
				continue;
			}

			// Normals with the inverse transpose; a mirroring transform turns the triangles inside out, so
			// their winding is flipped back.
			const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(world)));
			const bool flipWinding = glm::determinant(glm::mat3(world)) < 0.0f;

			const ImportedMesh& mesh = model.Meshes[node.MeshIndex];

			// A collision proxy: all its primitives' triangles, transformed like the drawn ones, kept apart.
			MeshCollisionKind collisionKind = MeshCollisionKind::Convex;
			if (GetCollisionKind(node.Name, collisionKind))
			{
				MeshCollisionProxy proxy;
				proxy.Name = node.Name;
				proxy.Kind = collisionKind;
				for (const ImportedPrimitive& primitive : mesh.Primitives)
				{
					const uint32_t baseVertex = (uint32_t)proxy.Positions.size();
					const uint32_t vertexCount = (uint32_t)primitive.Positions.size();
					for (const glm::vec3& position : primitive.Positions)
					{
						proxy.Positions.push_back(glm::vec3(world * glm::vec4(position, 1.0f)));
					}
					for (size_t t = 0; t + 2 < primitive.Indices.size(); t += 3)
					{
						uint32_t corner[3] = { primitive.Indices[t], primitive.Indices[t + 1], primitive.Indices[t + 2] };
						if (corner[0] >= vertexCount || corner[1] >= vertexCount || corner[2] >= vertexCount)
						{
							continue;
						}
						if (flipWinding)
						{
							std::swap(corner[1], corner[2]);
						}
						for (int c = 0; c < 3; c++)
						{
							proxy.Indices.push_back(baseVertex + corner[c]);
						}
					}
				}
				if (proxy.Indices.empty() == false)
				{
					data.CollisionProxies.push_back(std::move(proxy));
				}
				continue;
			}

			for (uint32_t p = 0; p < (uint32_t)mesh.Primitives.size(); p++)
			{
				const ImportedPrimitive& primitive = mesh.Primitives[p];
				const uint32_t baseVertex = (uint32_t)data.Vertices.size();
				const uint32_t vertexCount = (uint32_t)primitive.Positions.size();
				const bool hasNormals = primitive.Normals.size() == vertexCount;
				const bool generateNormals = hasNormals == false && config.GenerateNormals;

				SubMesh subMesh;
				subMesh.FirstIndex = (uint32_t)data.Indices.size();
				subMesh.MaterialSlot = primitive.Material >= 0 && primitive.Material < (int32_t)model.Materials.size() ? (uint32_t)primitive.Material : defaultSlot;
				subMesh.SourceNode = (int32_t)nodeIndex;
				subMesh.SourcePrimitive = p;
				subMesh.SourceName = node.Name;

				if (generateNormals)
				{
					// Flat: three vertices of its own per triangle.
					for (size_t t = 0; t + 2 < primitive.Indices.size(); t += 3)
					{
						uint32_t corner[3] = { primitive.Indices[t], primitive.Indices[t + 1], primitive.Indices[t + 2] };
						if (corner[0] >= vertexCount || corner[1] >= vertexCount || corner[2] >= vertexCount)
						{
							continue;
						}
						if (flipWinding)
						{
							std::swap(corner[1], corner[2]);
						}

						glm::vec3 positions[3];
						for (int c = 0; c < 3; c++)
						{
							positions[c] = glm::vec3(world * glm::vec4(primitive.Positions[corner[c]], 1.0f));
						}
						const glm::vec3 cross = glm::cross(positions[1] - positions[0], positions[2] - positions[0]);
						const float length = glm::length(cross);
						const glm::vec3 normal = length > 0.0f ? cross / length : glm::vec3(0.0f, 0.0f, 1.0f);

						for (int c = 0; c < 3; c++)
						{
							MeshVertex vertex;
							vertex.Position = positions[c];
							vertex.Normal = normal;
							vertex.UV = primitive.UVs.size() == vertexCount ? primitive.UVs[corner[c]] : glm::vec2(0.0f);
							vertex.Color = primitive.Colors.size() == vertexCount ? primitive.Colors[corner[c]] : glm::vec4(1.0f);
							data.Indices.push_back((uint32_t)data.Vertices.size());
							data.Vertices.push_back(vertex);
						}
					}
				}
				else
				{
					for (uint32_t v = 0; v < vertexCount; v++)
					{
						MeshVertex vertex;
						vertex.Position = glm::vec3(world * glm::vec4(primitive.Positions[v], 1.0f));
						if (hasNormals)
						{
							const glm::vec3 normal = normalMatrix * primitive.Normals[v];
							const float length = glm::length(normal);
							vertex.Normal = length > 0.0f ? normal / length : glm::vec3(0.0f, 0.0f, 1.0f);
						}
						vertex.UV = primitive.UVs.size() == vertexCount ? primitive.UVs[v] : glm::vec2(0.0f);
						vertex.Color = primitive.Colors.size() == vertexCount ? primitive.Colors[v] : glm::vec4(1.0f);
						data.Vertices.push_back(vertex);
					}
					for (size_t t = 0; t + 2 < primitive.Indices.size(); t += 3)
					{
						uint32_t corner[3] = { primitive.Indices[t], primitive.Indices[t + 1], primitive.Indices[t + 2] };
						if (corner[0] >= vertexCount || corner[1] >= vertexCount || corner[2] >= vertexCount)
						{
							continue;
						}
						if (flipWinding)
						{
							std::swap(corner[1], corner[2]);
						}
						for (int c = 0; c < 3; c++)
						{
							data.Indices.push_back(baseVertex + corner[c]);
						}
					}
				}

				subMesh.IndexCount = (uint32_t)data.Indices.size() - subMesh.FirstIndex;
				if (subMesh.IndexCount > 0)
				{
					data.SubMeshes.push_back(subMesh);
				}
			}
		}

		return data;
	}

	std::string MeshImporter::GetImageExtension(const std::string& mimeType)
	{
		if (mimeType == "image/png")
		{
			return ".png";
		}
		if (mimeType == "image/jpeg" || mimeType == "image/jpg")
		{
			return ".jpg";
		}
		if (mimeType == "image/bmp")
		{
			return ".bmp";
		}
		if (mimeType == "image/x-tga" || mimeType == "image/tga")
		{
			return ".tga";
		}
		return {};
	}
}
