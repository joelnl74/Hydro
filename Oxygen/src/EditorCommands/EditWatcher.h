#pragma once
#include "SceneEditCommand.h"

#include <Hydro/Scene/Scene.h>

#include <cstdint>
#include <string>

namespace Hydro
{
	struct EditorContext;

	// Undo steps for edits no window records itself: the selected entity's components (inspector, gizmo) and the scene's
	// render settings, compared once the user stops interacting, so a drag, a typed value or a click is one step.
	class EditWatcher
	{
	public:
		// Once per frame, after every window drew. Off while playing; picks up again after Stop.
		void Update(EditorContext& context);

		// "Edit Transform", "Add Sprite Renderer", "Rename": what changed between two captures of one entity.
		static std::string DescribeChange(const std::string& before, const std::string& after, const std::string& entityName);
		// "SpriteRendererComponent" -> "Sprite Renderer".
		static std::string GetComponentLabel(const std::string& key);

	private:
		void Resync(EditorContext& context);
		// Something the user did this frame could have changed a value (or ended an interaction).
		static bool HadInput();

	private:
		Ref<Scene> m_Scene;
		UUID m_Entity = UUID(0);
		std::string m_EntityComponents;
		std::string m_SceneSettings;
		uint64_t m_ChangeCount = 0;
		bool m_WasBusy = false;
		// Frames to keep resyncing after the selection changes (a window may fill in values on its first draw).
		int m_SettleFrames = 0;
	};
}
