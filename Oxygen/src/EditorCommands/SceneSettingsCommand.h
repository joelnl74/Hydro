#pragma once
#include "EditorCommand.h"

#include <Hydro/Scene/Scene.h>

#include <string>

namespace Hydro
{
	// A change to the scene's render settings, Look, environment or post-processing (the Render Settings window): the
	// YAML SceneSerializer::SerializeSettings writes, before and after.
	class SceneSettingsCommand : public EditorCommand
	{
	public:
		SceneSettingsCommand(const Ref<Scene>& scene, std::string before, std::string after);

		// The scene's settings as SceneSerializer saves them.
		static std::string Capture(const Scene& scene);

		void Undo() override { Apply(m_Before); }
		void Redo() override { Apply(m_After); }
		const char* GetName() const override { return "Edit Render Settings"; }
		size_t GetMemorySize() const override { return sizeof(*this) + m_Before.size() + m_After.size(); }

	private:
		void Apply(const std::string& settings) const;

	private:
		Ref<Scene> m_Scene;
		std::string m_Before;
		std::string m_After;
	};
}
