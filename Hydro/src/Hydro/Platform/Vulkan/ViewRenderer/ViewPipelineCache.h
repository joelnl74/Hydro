#pragma once
#include "Hydro/Core/Base.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

namespace Hydro
{
	class VulkanDescriptorBuilder;
	class VulkanPipeline;
	class VulkanRenderTarget;

	// What a view pipeline draws; its blending, depth and entity-ID behaviour follow from the kind.
	enum class ViewPipelineKind : uint8_t
	{
		Sprite = 0,     // translucent quads: depth test (no write) when there is depth
		Decal,          // ground decals: like Sprite, with a small depth bias towards the camera
		Cutout,         // cut-out quads: depth test and write, alpha < 0.5 discarded, no blending
		FadedCutout,    // cut-out quads being faded: like Cutout, screen-door faded by the quad's alpha
		Overlay,        // overlay quads (editor outlines, HUD sprites): no depth test
		Composite,      // the 2D light composite
		DebugLines,     // editor lines in the scene: depth test, no depth write, never IDs
		ShadowCaster,   // sun shadow casters: depth only, alpha tested, into a view's shadow map
		Tonemap,        // the HDR scene colour through exposure and the tonemap, a fullscreen triangle
		BloomPrefilter, // bloom: the scene colour to half resolution, soft threshold
		BloomDown,      // bloom: one level to the next, half the size
		BloomUp,        // bloom: one level, filtered, added onto the level above it
		DofCoc,         // depth of field: the signed circle of confusion from the shared depth, full size
		DofPrepare,     // depth of field: the scene colour and its circle of confusion at half size
		DofBlur,        // depth of field: the gather blur, half size
		Additive        // additive quads: depth test (no write) when there is depth, colour added, unlit
	};

	// The VulkanShaderLibrary names of the shaders view pipelines are built from.
	static constexpr const char* SpriteShaderName = "Sprite";
	static constexpr const char* SpriteLitShaderName = "SpriteLit";
	static constexpr const char* SpriteCutoutShaderName = "SpriteCutout";
	static constexpr const char* SpriteCutoutLitShaderName = "SpriteCutoutLit";
	static constexpr const char* SpriteFadeShaderName = "SpriteFade";
	static constexpr const char* SpriteFadeLitShaderName = "SpriteFadeLit";
	static constexpr const char* SpriteShadowCasterShaderName = "SpriteShadowCaster";
	static constexpr const char* LightShaderName = "Light";
	static constexpr const char* ShadowShaderName = "Shadow";
	static constexpr const char* CompositeShaderName = "Composite";
	static constexpr const char* TonemapShaderName = "Tonemap";
	static constexpr const char* BloomPrefilterShaderName = "BloomPrefilter";
	static constexpr const char* BloomDownShaderName = "BloomDown";
	static constexpr const char* BloomUpShaderName = "BloomUp";
	static constexpr const char* DofCocShaderName = "DofCoc";
	static constexpr const char* DofPrepareShaderName = "DofPrepare";
	static constexpr const char* DofBlurShaderName = "DofBlur";

	// Builds the pipelines that draw into views, one per attachment layout and kind, and keeps them until a
	// shader or the wireframe setting changes. Owns the quad pipeline they start from and their set layouts.
	class ViewPipelineCache
	{
	public:
		static void Init();
		static void Shutdown();

		// The pipeline that draws `kind` into `target`, or null when it can't be built (logged once).
		// `colorTarget`: the pass draws into that target's colour instead (the HDR scene colour).
		static VulkanPipeline* Get(ViewPipelineKind kind, const VulkanRenderTarget& target, bool writeEntityID, bool lit = false,
			const VulkanRenderTarget* colorTarget = nullptr);

		// Rebuilds for the wireframe setting and for shaders changed on disk (RendererSettings::ShaderHotReload).
		static void ApplySettings();
		// Rebuilds `pipeline` with the library's current `shaderName`; the old one keeps drawing on failure.
		static void RebuildWithShader(Scope<VulkanPipeline>& pipeline, const char* shaderName);

		// Set 0 of the quad and mesh shaders, which every view's descriptor sets are built like.
		static VulkanDescriptorBuilder& GetLayoutDescriptorSet();
		static VkDescriptorSetLayout GetTonemapLayout();
		static VkDescriptorSetLayout GetBloomLayout();
		static VkDescriptorSetLayout GetDepthOfFieldLayout();

	private:
		static void Clear();
		static void Clear(const std::string& shaderName);
		static void ReloadShadersIfChanged();
		static bool RebuildQuadPipeline(bool wireframe);
	};
}
