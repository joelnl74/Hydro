#pragma once
#include "Hydro/Scene/Entity.h"
#include "TextureImportPanel.h"
#include "Hydro/Scene/SpriteAnimation.h"
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Renderer/Material.h"
#include "Hydro/Renderer/Mesh.h"
#include "Hydro/Renderer/MeshImporter.h"

#include <filesystem>
#include <functional>

namespace Hydro
{
	// Shows and edits the components of the entity selected in the hierarchy, or, when no entity is
	// selected, the asset selected in the Project window (an image's import settings, a .hyanim's clips, a
	// material's fields).
	// Editor UI only: it reads and writes component data, it holds no behaviour of its own.
	class InspectorWindow
	{
	public:
		// Set by the editor layer.
		std::function<void(const std::filesystem::path&)> OnTextureReimported;
		std::function<void(const std::filesystem::path&)> OnOpenSpriteEditor;
		std::function<void(const std::filesystem::path&)> OnOpenAnimation;

		InspectorWindow() = default;
		~InspectorWindow() = default;

		// `asset` is shown when `entity` isn't valid (empty: nothing).
		void OnImguiRender(Entity entity, const std::filesystem::path& asset);

	private:
		void DrawEntityHeader(Entity entity);
		void DrawComponents(Entity entity);
		void DrawAddComponentButton(Entity entity);
		// The runtime UI's sections (canvas, rect, image, text, button); removals are requested like DrawComponents'.
		void DrawUIComponents(Entity entity, bool& removeCanvas, bool& removeRect, bool& removeImage, bool& removeText, bool& removeButton);
		// One menu item per registered script; picking one binds it to the entity (adding its
		// NativeScriptComponent if needed).
		void DrawScriptMenuItems(Entity entity);
		void DrawPrefabButton(Entity entity);
		void DrawAsset(const std::filesystem::path& asset);
		void DrawAnimationSetAsset(const std::filesystem::path& path);
		void DrawMaterialAsset(const std::filesystem::path& path);
		// A selected .hyrender: its environment and post-processing (RenderLookUI), shared by every scene using it.
		void DrawRenderProfileAsset(const std::filesystem::path& path);
		// A mesh's material slot: its material asset (picked or dropped), whose fields edit the shared asset,
		// or its inline settings with "Save as Material Asset".
		void DrawMeshMaterialSlot(Entity entity, MeshMaterialSlot& slot);
		// Every slot of a mesh (a tree node per slot for models, named after the model's materials).
		void DrawMeshMaterialSlots(Entity entity, MeshComponent& mesh, const Mesh* model);
		// Gives `mesh` one slot per material of `model`, each using the material asset its import made.
		static void AssignModelMaterials(MeshComponent& mesh, const Mesh& model);
		// A selected .gltf / .glb: its import settings (Apply re-imports) and what it holds.
		void DrawModelAsset(const std::filesystem::path& path);
		// The fields of a material asset; a change applies to the loaded asset at once (every mesh using it)
		// and marks it to be saved.
		void EditMaterialAsset(const Ref<MaterialAsset>& asset);
		// Writes the material edited last to its .hymat once no field is active (`force`: now).
		void SaveEditedMaterial(bool force);

	private:
		TextureImportPanel m_TextureImportPanel;
		// Held so the shared set isn't loaded again every frame (LoadFromFile shares only live sets).
		Ref<SpriteAnimationSet> m_InspectedAnimationSet;
		std::filesystem::path m_InspectedAnimationPath;
		std::filesystem::file_time_type m_InspectedAnimationTime{};
		// The material asset changed in memory and not written yet (0: none).
		AssetHandle m_UnsavedMaterial = 0;
		// The import settings being edited for the selected model (read again when another one is selected).
		std::filesystem::path m_ModelSettingsPath;
		MeshImportConfig m_ModelSettings;
	};
}
