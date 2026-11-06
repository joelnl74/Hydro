#include "EditWatcher.h"
#include "SceneSettingsCommand.h"
#include "EditorContext.h"

#include <imgui/imgui.h>

#include <Hydro/Utils/YAMLConversions.h>

#include <cctype>
#include <set>
#include <vector>

namespace Hydro
{
	void EditWatcher::Resync(EditorContext& context)
	{
		m_Scene = context.EditorScene;
		m_Entity = context.GetSelectedUUID();
		m_EntityComponents.clear();
		if ((uint64_t)m_Entity != 0)
		{
			m_EntityComponents = EntityState::SerializeComponents(context.Selection);
		}
		m_SceneSettings = m_Scene != nullptr ? SceneSettingsCommand::Capture(*m_Scene) : std::string();
		m_ChangeCount = context.Commands.GetChangeCount();
		m_WasBusy = false;
	}

	bool EditWatcher::HadInput()
	{
		const ImGuiIO& io = ImGui::GetIO();
		if (io.InputQueueCharacters.Size > 0 || io.MouseWheel != 0.0f || io.MouseWheelH != 0.0f)
		{
			return true;
		}
		for (int button = 0; button < ImGuiMouseButton_COUNT; button++)
		{
			if (ImGui::IsMouseClicked(button) || ImGui::IsMouseReleased(button))
			{
				return true;
			}
		}
		for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; key++)
		{
			if (ImGui::IsKeyPressed((ImGuiKey)key, false) || ImGui::IsKeyReleased((ImGuiKey)key))
			{
				return true;
			}
		}
		return false;
	}

	void EditWatcher::Update(EditorContext& context)
	{
		if (context.Playing || context.EditorScene == nullptr)
		{
			m_Scene = nullptr;
			return;
		}

		// Another scene, another selection, or the history moved (an edit recorded elsewhere, an undo): start over.
		if (context.EditorScene != m_Scene || context.GetSelectedUUID() != m_Entity)
		{
			Resync(context);
			m_SettleFrames = 1;
			return;
		}
		if (context.Commands.GetChangeCount() != m_ChangeCount)
		{
			Resync(context);
			return;
		}

		// Mid-interaction: a drag, a field being typed in, the gizmo held. One step once it ends.
		const bool busy = ImGui::IsAnyItemActive() || ImGui::IsAnyMouseDown();
		if (busy)
		{
			m_WasBusy = true;
			return;
		}
		if (m_SettleFrames > 0)
		{
			m_SettleFrames--;
			Resync(context);
			return;
		}
		const bool interacted = m_WasBusy || HadInput();
		m_WasBusy = false;
		if (interacted == false)
		{
			return;
		}

		if ((uint64_t)m_Entity != 0)
		{
			Entity entity = context.Selection;
			const std::string components = EntityState::SerializeComponents(entity);
			if (components != m_EntityComponents)
			{
				// Only the components changed: both states keep the entity where it is now.
				EntityState after = EntityState::Capture(*m_Scene, m_Entity);
				EntityState before = after;
				before.Components = m_EntityComponents;
				const std::string name = DescribeChange(m_EntityComponents, components, entity.GetName());
				context.Commands.Push(CreateScope<SceneEditCommand>(m_Scene, name, std::vector<EntityState>{ before },
					std::vector<EntityState>{ after }, m_Entity, m_Entity));
				m_EntityComponents = components;
			}
		}

		const std::string settings = SceneSettingsCommand::Capture(*m_Scene);
		if (settings != m_SceneSettings)
		{
			context.Commands.Push(CreateScope<SceneSettingsCommand>(m_Scene, m_SceneSettings, settings));
			m_SceneSettings = settings;
		}
		m_ChangeCount = context.Commands.GetChangeCount();
	}

	std::string EditWatcher::GetComponentLabel(const std::string& key)
	{
		if (key == "NativeScriptComponent")
		{
			return "Scripts";
		}
		std::string name = key;
		const std::string suffix = "Component";
		if (name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
		{
			name.resize(name.size() - suffix.size());
		}
		// A space before a capital that starts a word: "SpriteRenderer" -> "Sprite Renderer", "UIRect" -> "UI Rect".
		std::string label;
		for (size_t i = 0; i < name.size(); i++)
		{
			const unsigned char c = (unsigned char)name[i];
			if (i > 0 && std::isupper(c))
			{
				const unsigned char previous = (unsigned char)name[i - 1];
				const bool nextLower = i + 1 < name.size() && std::islower((unsigned char)name[i + 1]);
				if (std::islower(previous) || (std::isupper(previous) && nextLower))
				{
					label += ' ';
				}
			}
			label += (char)c;
		}
		return label;
	}

	std::string EditWatcher::DescribeChange(const std::string& before, const std::string& after, const std::string& entityName)
	{
		const YAML::Node a = YAML::Load(before);
		const YAML::Node b = YAML::Load(after);
		std::set<std::string> keys;
		for (const YAML::Node* node : { &a, &b })
		{
			if (node->IsMap())
			{
				for (const auto& entry : *node)
				{
					keys.insert(entry.first.as<std::string>());
				}
			}
		}

		std::vector<std::string> added;
		std::vector<std::string> removed;
		std::vector<std::string> changed;
		for (const std::string& key : keys)
		{
			if (key == "Entity")
			{
				continue;
			}
			const bool inBefore = a[key].IsDefined();
			const bool inAfter = b[key].IsDefined();
			if (inBefore && inAfter == false)
			{
				removed.push_back(key);
			}
			else if (inAfter && inBefore == false)
			{
				added.push_back(key);
			}
			else
			{
				YAML::Emitter ea;
				ea << a[key];
				YAML::Emitter eb;
				eb << b[key];
				if (std::string(ea.c_str()) != eb.c_str())
				{
					changed.push_back(key);
				}
			}
		}

		const size_t total = added.size() + removed.size() + changed.size();
		if (total == 1 && changed.size() == 1 && changed[0] == "TagComponent")
		{
			return "Rename " + entityName;
		}
		if (total == 1)
		{
			if (added.size() == 1)
			{
				return "Add " + GetComponentLabel(added[0]);
			}
			if (removed.size() == 1)
			{
				return "Remove " + GetComponentLabel(removed[0]);
			}
			return "Edit " + GetComponentLabel(changed[0]);
		}
		return "Edit " + entityName;
	}
}
