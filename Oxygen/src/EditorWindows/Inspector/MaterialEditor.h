#pragma once
#include <Hydro/Core/Base.h>
#include <Hydro/Renderer/Material.h>
#include <Hydro/Renderer/Mesh.h>
#include <Hydro/Scene/Entity.h>
#include <Hydro/Scene/Components/Components.h>

namespace Hydro
{
	// The Standard material's fields, for a mesh's slots and a selected .hymat. A material asset's edit applies at once
	// to every mesh using it; the file is written once the edit is finished.
	class MaterialEditor
	{
	public:
		// A material asset's fields; an edit marks it to be saved.
		void EditAsset(const Ref<MaterialAsset>& asset);
		// Writes the material edited last to its .hymat once no field is active (`force`: now).
		void Save(bool force);
		// Every slot of a mesh, one tree node per slot for models.
		void DrawMeshSlots(Entity entity, MeshComponent& mesh, const Mesh* model);
		// Gives `mesh` one slot per material of `model`, each using the material asset its import made.
		static void AssignModelMaterials(MeshComponent& mesh, const Mesh& model);

	private:
		// A mesh's material slot: its material asset (whose fields edit the shared asset) or its inline settings.
		void DrawMeshSlot(Entity entity, MeshMaterialSlot& slot);

	private:
		// Changed in memory and not written yet (0: none).
		AssetHandle m_UnsavedMaterial = 0;
	};
}
