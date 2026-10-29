#include "HierarchyWindow.h"
#include "../EditorPaths.h"
#include "Utilities/AssetDragDrop.h"

#include <imgui/imgui.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <vector>
#include <Hydro/Core/UUID.h>
#include <Hydro/Scene/PrefabSerializer.h>
#include <Hydro/Utils/PlatformUtils.h>

namespace Hydro
{
	// Drag-and-drop payload of a hierarchy row: the dragged entt::entity.
	static const char* s_EntityPayloadType = "HYDRO_HIERARCHY_ENTITY";

	// The share of a row's height, at its top and its bottom, that drops before / after it as a sibling; the
	// middle makes the dragged entity its child.
	static constexpr float SiblingDropEdge = 0.25f;

	HierarchyWindow::HierarchyWindow()
	{
	}

	HierarchyWindow::~HierarchyWindow()
	{
	}

	bool HierarchyWindow::MatchesSearch(const std::string& name) const
	{
		const size_t length = std::char_traits<char>::length(m_SearchBuffer);
		if (length == 0)
		{
			return true;
		}

		const auto lower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
		return std::search(name.begin(), name.end(), m_SearchBuffer, m_SearchBuffer + length,
			[&](char a, char b) { return lower(a) == lower(b); }) != name.end();
	}

	void HierarchyWindow::OnImguiAttach()
	{
	}

	void HierarchyWindow::OnImguiRender(Ref<Scene> scene)
	{
		// Drop a selection that belongs to a scene that was replaced or an entity that was destroyed.
		if (m_SelectedEntity && (m_SelectedEntity.GetScene() != scene.get() || scene->m_Registry.valid(m_SelectedEntity) == false))
		{
			m_SelectedEntity = {};
		}
		m_Pending = {};

		// "###Hierachy" keeps the window's ImGui ID from before the rename, so saved docking layouts still find it.
		ImGui::Begin("Hierarchy###Hierachy");

		// Search: only rows whose name contains the text are listed (flat, without folding).
		{
			const float clearWidth = ImGui::GetFrameHeight();
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - clearWidth - ImGui::GetStyle().ItemSpacing.x);
			ImGui::InputTextWithHint("##HierarchySearch", "Search...", m_SearchBuffer, sizeof(m_SearchBuffer));
			ImGui::SameLine();
			ImGui::BeginDisabled(m_SearchBuffer[0] == '\0');
			if (ImGui::Button("x", ImVec2(clearWidth, 0.0f)))
			{
				m_SearchBuffer[0] = '\0';
			}
			ImGui::EndDisabled();
		}
		const bool searching = m_SearchBuffer[0] != '\0';

		const bool rootOpen = ImGui::TreeNodeEx("Root", ImGuiTreeNodeFlags_DefaultOpen);

		// Drop a prefab from the project window onto "Root" to instantiate it, or a row to make it a root.
		if (ImGui::BeginDragDropTarget())
		{
			std::filesystem::path droppedPath;
			if (AssetDragDrop::AcceptPayload(droppedPath) && droppedPath.extension().string() == PrefabSerializer::FileExtension)
			{
				m_Pending = PendingEdit::Instantiating(droppedPath, {});
			}
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(s_EntityPayloadType))
			{
				const entt::entity dragged = *(const entt::entity*)payload->Data;
				if (scene->m_Registry.valid(dragged))
				{
					m_Pending = PendingEdit::Of(PendingEdit::Type::MakeRoot, Entity{ dragged, scene.get() });
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (rootOpen)
		{
			// A copy: the scene's list must not change under the loop (edits wait in m_Pending anyway).
			const std::vector<entt::entity> order = scene->GetEntityOrder();
			// While folded rows' descendants are skipped: rows deeper than this are hidden.
			uint32_t hideDeeperThan = UINT32_MAX;
			for (entt::entity handle : order)
			{
				if (scene->m_Registry.valid(handle) == false || scene->m_Registry.has<TagComponent>(handle) == false
					|| scene->m_Registry.has<IDComponent>(handle) == false)
				{
					continue;
				}

				Entity entity{ handle, scene.get() };
				const uint32_t depth = scene->GetDepth(entity);
				if (searching)
				{
					if (MatchesSearch(entity.GetName()) == false)
					{
						continue;
					}
				}
				else
				{
					if (depth > hideDeeperThan)
					{
						continue;
					}
					hideDeeperThan = UINT32_MAX;
				}

				bool open = true;
				DrawRow(*scene, entity, searching, open);
				if (searching == false && open == false)
				{
					hideDeeperThan = depth;
				}
			}

			ImGui::TreePop();
		}

		// Right-click on empty space in the window.
		if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("Create Empty Entity"))
			{
				m_Pending = PendingEdit::Creating(PendingEdit::Template::Empty, {});
			}

			if (ImGui::MenuItem("Create Sprite"))
			{
				m_Pending = PendingEdit::Creating(PendingEdit::Template::Sprite, {});
			}

			if (ImGui::MenuItem("Create Light"))
			{
				m_Pending = PendingEdit::Creating(PendingEdit::Template::Light, {});
			}

			if (ImGui::BeginMenu("Create UI"))
			{
				DrawCreateUIMenu({});
				ImGui::EndMenu();
			}

			if (ImGui::MenuItem("Create Camera"))
			{
				m_SelectedEntity = scene->CreateEntity("Camera");
				m_SelectedEntity.AddComponent<TransformComponent>();
				// Only the first camera in a scene is primary by default.
				m_SelectedEntity.AddComponent<CameraComponent>().Primary = (bool)scene->GetPrimaryCameraEntity() == false;
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Instantiate Prefab..."))
			{
				std::string filepath = FileDialogs::OpenFile("Hydro Prefab (*.hyprefab)\0*.hyprefab\0");
				if (filepath.empty() == false)
				{
					m_Pending = PendingEdit::Instantiating(filepath, {});
				}
			}

			ImGui::EndPopup();
		}

		ImGui::End();

		ApplyPendingEdit(*scene);
	}

	void HierarchyWindow::DrawRow(Scene& scene, Entity entity, bool searching, bool& open)
	{
		const std::string& name = entity.GetName();
		const uint64_t id = entity.GetUUID();
		const uint32_t depth = searching ? 0 : scene.GetDepth(entity);
		const bool hasChildren = scene.HasChildren(entity);

		// The list is flat (depth-first), so rows are indented by their depth rather than nested tree nodes.
		const float indent = (float)depth * ImGui::GetStyle().IndentSpacing;
		if (indent > 0.0f)
		{
			ImGui::Indent(indent);
		}

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow
			| ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_DefaultOpen;
		if (hasChildren == false || searching)
		{
			flags |= ImGuiTreeNodeFlags_Leaf;
		}
		if (m_SelectedEntity == entity)
		{
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		// The ID is the entity's UUID, so a row stays folded or open while it moves around the tree.
		open = ImGui::TreeNodeEx((void*)id, flags, "%s", name.c_str());
		// Clicking the arrow only folds or unfolds.
		if (ImGui::IsItemClicked() && ImGui::IsItemToggledOpen() == false)
		{
			m_SelectedEntity = entity;
		}
		// Double-click: select it and look at it in the scene view.
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			m_SelectedEntity = entity;
			if (OnEntityFocus)
			{
				OnEntityFocus(entity);
			}
		}

		if (ImGui::BeginDragDropSource())
		{
			const entt::entity handle = entity;
			ImGui::SetDragDropPayload(s_EntityPayloadType, &handle, sizeof(entt::entity));
			ImGui::TextUnformatted(name.c_str());
			ImGui::EndDragDropSource();
		}

		DrawRowDropTarget(scene, entity);
		DrawRowContextMenu(scene, entity);

		if (indent > 0.0f)
		{
			ImGui::Unindent(indent);
		}
	}

	void HierarchyWindow::DrawRowDropTarget(Scene& scene, Entity entity)
	{
		if (ImGui::BeginDragDropTarget() == false)
		{
			return;
		}

		// A prefab dropped on a row: a new instance as its last child.
		std::filesystem::path droppedPath;
		if (AssetDragDrop::AcceptPayload(droppedPath) && droppedPath.extension().string() == PrefabSerializer::FileExtension)
		{
			m_Pending = PendingEdit::Instantiating(droppedPath, entity);
		}

		const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(s_EntityPayloadType,
			ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
		if (payload != nullptr && payload->DataSize == sizeof(entt::entity))
		{
			const entt::entity draggedHandle = *(const entt::entity*)payload->Data;
			const Entity dragged{ draggedHandle, &scene };
			// Onto itself or something inside it: nowhere to go.
			const bool allowed = scene.m_Registry.valid(draggedHandle) && dragged != entity && scene.IsAncestorOf(dragged, entity) == false;
			if (allowed)
			{
				const ImVec2 rowMin = ImGui::GetItemRectMin();
				const ImVec2 rowMax = ImGui::GetItemRectMax();
				const float edge = (rowMax.y - rowMin.y) * SiblingDropEdge;
				const float mouseY = ImGui::GetMousePos().y;
				PendingEdit::Type kind = PendingEdit::Type::MakeChild;
				if (mouseY < rowMin.y + edge)
				{
					kind = PendingEdit::Type::MoveBefore;
				}
				else if (mouseY > rowMax.y - edge)
				{
					kind = PendingEdit::Type::MoveAfter;
				}

				// A line where it will land as a sibling, a frame around the row it will be a child of.
				const ImU32 color = ImGui::GetColorU32(ImGuiCol_DragDropTarget);
				ImDrawList* drawList = ImGui::GetWindowDrawList();
				if (kind == PendingEdit::Type::MakeChild)
				{
					drawList->AddRect(rowMin, rowMax, color, 0.0f, 0, 2.0f);
				}
				else
				{
					const float lineY = kind == PendingEdit::Type::MoveAfter ? rowMax.y : rowMin.y;
					drawList->AddLine(ImVec2(rowMin.x, lineY), ImVec2(rowMax.x, lineY), color, 2.0f);
				}

				if (payload->IsDelivery())
				{
					m_Pending = PendingEdit::Of(kind, dragged, entity);
				}
			}
		}

		ImGui::EndDragDropTarget();
	}

	void HierarchyWindow::DrawRowContextMenu(Scene& scene, Entity entity)
	{
		if (ImGui::BeginPopupContextItem() == false)
		{
			return;
		}

		if (ImGui::BeginMenu("Create Child"))
		{
			if (ImGui::MenuItem("Empty"))
			{
				m_Pending = PendingEdit::Creating(PendingEdit::Template::Empty, entity);
			}
			if (ImGui::MenuItem("Sprite"))
			{
				m_Pending = PendingEdit::Creating(PendingEdit::Template::Sprite, entity);
			}
			if (ImGui::MenuItem("Light"))
			{
				m_Pending = PendingEdit::Creating(PendingEdit::Template::Light, entity);
			}
			if (ImGui::BeginMenu("UI"))
			{
				DrawCreateUIMenu(entity);
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if ((bool)scene.GetParent(entity) && ImGui::MenuItem("Unparent"))
		{
			m_Pending = PendingEdit::Of(PendingEdit::Type::MakeRoot, entity);
		}

		ImGui::Separator();

		const bool hasChildren = scene.HasChildren(entity);
		if (ImGui::MenuItem(hasChildren ? "Save as Prefab (with children)" : "Save as Prefab"))
		{
			SaveAsPrefab(entity);
		}
		if (ImGui::MenuItem(hasChildren ? "Duplicate (with children)" : "Duplicate Entity"))
		{
			m_Pending = PendingEdit::Of(PendingEdit::Type::Duplicate, entity);
		}
		if (ImGui::MenuItem(hasChildren ? "Delete (with children)" : "Delete Entity"))
		{
			m_Pending = PendingEdit::Of(PendingEdit::Type::Delete, entity);
		}

		ImGui::EndPopup();
	}

	void HierarchyWindow::DrawCreateUIMenu(Entity parent)
	{
		struct Item
		{
			const char* Name;
			PendingEdit::Template Kind;
		};
		const Item items[] = {
			{ "Canvas", PendingEdit::Template::UICanvas },
			{ "Panel", PendingEdit::Template::UIPanel },
			{ "Image", PendingEdit::Template::UIImage },
			{ "Text", PendingEdit::Template::UIText },
			{ "Button", PendingEdit::Template::UIButton } };
		for (const Item& item : items)
		{
			if (ImGui::MenuItem(item.Name))
			{
				m_Pending = PendingEdit::Creating(item.Kind, parent);
			}
		}
	}

	Entity HierarchyWindow::CreateUIFromTemplate(Scene& scene, PendingEdit::Template kind, Entity parent)
	{
		if (kind == PendingEdit::Template::UICanvas)
		{
			Entity canvas = scene.CreateEntity("Canvas");
			canvas.AddComponent<UICanvasComponent>();
			if (parent)
			{
				scene.SetParent(canvas, parent, false);
			}
			return canvas;
		}

		// Elements sit on a canvas: the one at or above `parent`, else a new one.
		bool onCanvas = false;
		for (Entity current = parent; (bool)current; current = scene.GetParent(current))
		{
			if (current.HasComponent<UICanvasComponent>())
			{
				onCanvas = true;
				break;
			}
		}
		if (onCanvas == false)
		{
			parent = CreateUIFromTemplate(scene, PendingEdit::Template::UICanvas, {});
		}

		const char* name = "Image";
		switch (kind)
		{
		case PendingEdit::Template::UIPanel: name = "Panel"; break;
		case PendingEdit::Template::UIText: name = "Text"; break;
		case PendingEdit::Template::UIButton: name = "Button"; break;
		default: break;
		}

		Entity element = scene.CreateEntity(name);
		UIRectComponent& rect = element.AddComponent<UIRectComponent>();
		switch (kind)
		{
		case PendingEdit::Template::UIPanel:
			rect.SizeDelta = { 200.0f, 120.0f };
			element.AddComponent<UIImageComponent>().Color = { 0.08f, 0.07f, 0.06f, 0.85f };
			break;
		case PendingEdit::Template::UIText:
			rect.SizeDelta = { 160.0f, 24.0f };
			element.AddComponent<UITextComponent>().Text = "Text";
			break;
		case PendingEdit::Template::UIButton:
		{
			rect.SizeDelta = { 96.0f, 24.0f };
			element.AddComponent<UIImageComponent>().Color = { 0.25f, 0.22f, 0.18f, 1.0f };
			element.AddComponent<UIButtonComponent>();
			// Its label fills it.
			Entity label = scene.CreateEntity("Text");
			UIRectComponent& labelRect = label.AddComponent<UIRectComponent>();
			labelRect.AnchorMin = { 0.0f, 0.0f };
			labelRect.AnchorMax = { 1.0f, 1.0f };
			labelRect.SizeDelta = { 0.0f, 0.0f };
			UITextComponent& text = label.AddComponent<UITextComponent>();
			text.Text = "Button";
			text.AlignH = UITextAlignH::Center;
			text.AlignV = UITextAlignV::Middle;
			text.Wrap = false;
			scene.SetParent(label, element, false);
			break;
		}
		default:
			rect.SizeDelta = { 64.0f, 64.0f };
			element.AddComponent<UIImageComponent>();
			break;
		}

		scene.SetParent(element, parent, false);
		return element;
	}

	Entity HierarchyWindow::CreateFromTemplate(Scene& scene, PendingEdit::Template kind, Entity parent)
	{
		if (kind >= PendingEdit::Template::UICanvas)
		{
			return CreateUIFromTemplate(scene, kind, parent);
		}

		const char* name = kind == PendingEdit::Template::Sprite ? "Sprite" : (kind == PendingEdit::Template::Light ? "Light" : "Empty Entity");
		Entity entity = scene.CreateEntity(name);
		entity.AddComponent<TransformComponent>();
		if (kind == PendingEdit::Template::Sprite)
		{
			entity.AddComponent<SpriteRendererComponent>(glm::vec4(0.9f, 0.4f, 0.2f, 1.0f));
		}
		else if (kind == PendingEdit::Template::Light)
		{
			entity.AddComponent<Light2DComponent>();
		}

		// At the parent's origin: its (identity) transform is taken as local.
		if (parent)
		{
			scene.SetParent(entity, parent, false);
		}
		return entity;
	}

	void HierarchyWindow::ApplyPendingEdit(Scene& scene)
	{
		const PendingEdit edit = m_Pending;
		m_Pending = {};
		switch (edit.Kind)
		{
		case PendingEdit::Type::MoveBefore:
		case PendingEdit::Type::MoveAfter:
			scene.MoveEntity(edit.Subject, edit.Target, edit.Kind == PendingEdit::Type::MoveAfter);
			break;
		case PendingEdit::Type::MakeChild:
			scene.SetParent(edit.Subject, edit.Target);
			break;
		case PendingEdit::Type::MakeRoot:
			scene.SetParent(edit.Subject, {});
			break;
		case PendingEdit::Type::Duplicate:
			if (Entity duplicate = scene.DuplicateEntity(edit.Subject))
			{
				m_SelectedEntity = duplicate;
			}
			break;
		case PendingEdit::Type::Delete:
			// Its children go with it, so the selection may be one of them.
			if (m_SelectedEntity && (m_SelectedEntity == edit.Subject || scene.IsAncestorOf(edit.Subject, m_SelectedEntity)))
			{
				m_SelectedEntity = {};
			}
			scene.DestroyEntity(edit.Subject);
			break;
		case PendingEdit::Type::CreateChild:
			m_SelectedEntity = CreateFromTemplate(scene, edit.Create, edit.Target);
			break;
		case PendingEdit::Type::InstantiatePrefab:
			if (Entity instance = PrefabSerializer::Instantiate(scene, edit.Prefab, edit.Target))
			{
				m_SelectedEntity = instance;
			}
			break;
		default:
			break;
		}
	}

	void HierarchyWindow::SaveAsPrefab(Entity entity)
	{
		PrefabSerializer::Save(entity, EditorPaths::GetPrefabPath(entity.GetName()));
	}
}
