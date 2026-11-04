#include "AssetFileActions.h"
#include "EditorWindows/SpriteAnimationWindow.h"

#include <imgui/imgui.h>

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Core/Log.h>
#include <Hydro/Data/DataTableAsset.h>
#include <Hydro/Data/DefinitionAsset.h>
#include <Hydro/Renderer/Material.h>
#include <Hydro/Renderer/MaterialSerializer.h>
#include <Hydro/Renderer/RenderProfile.h>
#include <Hydro/Renderer/RenderProfileSerializer.h>
#include <Hydro/Renderer/Texture2D.h>
#include <Hydro/Scene/SpriteAnimation.h>
#include <Hydro/Scene/SpriteAnimationSerializer.h>

#include <cstdio>
#include <system_error>

namespace Hydro
{
	void AssetFileActions::BeginCreateFolder(const std::filesystem::path& folder)
	{
		// Suggest a name that doesn't exist yet; the user can type over it.
		const std::string name = GetFreePath(folder, "New Folder", std::string()).filename().u8string();

		m_CreateFolder = folder;
		m_RenamePath.clear();
		m_RenameExtension.clear();
		std::snprintf(m_NameBuffer, sizeof(m_NameBuffer), "%s", name.c_str());
		m_RequestNamePopup = true;
	}

	void AssetFileActions::BeginRename(const std::filesystem::path& path)
	{
		// Folders: the whole name. Files: the name without the extension, which stays as it is.
		std::error_code ec;
		const bool isFolder = std::filesystem::is_directory(path, ec);
		m_RenamePath = path;
		m_RenameExtension = isFolder ? std::string() : path.extension().u8string();
		const std::string name = isFolder ? path.filename().u8string() : path.stem().u8string();
		std::snprintf(m_NameBuffer, sizeof(m_NameBuffer), "%s", name.c_str());
		m_RequestNamePopup = true;
	}

	void AssetFileActions::BeginDelete(const std::filesystem::path& path)
	{
		m_PendingDeletePath = path;
		m_RequestDeletePopup = true;
	}

	void AssetFileActions::DrawPopups()
	{
		DrawDeletePopup();
		DrawNamePopup();
	}

	bool AssetFileActions::CanRename(const std::filesystem::path& path)
	{
		std::error_code ec;
		return std::filesystem::is_directory(path, ec) || static_cast<uint64_t>(AssetManager::FindHandle(path)) != 0;
	}

	std::filesystem::path AssetFileActions::GetFreePath(const std::filesystem::path& folder, const std::string& base,
		const std::string& extension)
	{
		std::filesystem::path target = folder / std::filesystem::u8path(base + extension);
		std::error_code error;
		for (int number = 2; std::filesystem::exists(target, error); number++)
		{
			target = folder / std::filesystem::u8path(base + " " + std::to_string(number) + extension);
		}
		return target;
	}

	std::string AssetFileActions::ValidateName(const std::string& name, const std::string& extension,
		const std::filesystem::path& parent, const std::filesystem::path& ignore)
	{
		if (name.empty())
		{
			return "Enter a name.";
		}
		if (name == "." || name == "..")
		{
			return "That name is reserved.";
		}
		if (name.front() == ' ' || name.back() == ' ' || name.back() == '.')
		{
			return "Names can't start or end with a space, or end with a dot.";
		}
		for (char c : name)
		{
			if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*'
				|| (unsigned char)c < 32)
			{
				return "Names can't contain < > : \" / \\ | ? *";
			}
		}

		// Windows file names aren't case sensitive, so "textures" clashes with "Textures".
		const std::filesystem::path target = parent / std::filesystem::u8path(name + extension);
		std::error_code ec;
		if (std::filesystem::exists(target, ec) && (ignore.empty() || std::filesystem::equivalent(target, ignore, ec) == false))
		{
			return "Something with that name already exists here.";
		}

		return {};
	}

	void AssetFileActions::DrawDeletePopup()
	{
		if (m_RequestDeletePopup)
		{
			ImGui::OpenPopup("Delete Asset?");
			m_RequestDeletePopup = false;
		}

		if (ImGui::BeginPopupModal("Delete Asset?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			std::error_code ec;
			const bool isFolder = std::filesystem::is_directory(m_PendingDeletePath, ec);
			const bool canDelete = isFolder == false || std::filesystem::is_empty(m_PendingDeletePath, ec);

			ImGui::Text("Delete '%s'?", m_PendingDeletePath.filename().u8string().c_str());
			if (canDelete)
			{
				ImGui::TextDisabled("This cannot be undone. Scenes that use it will report it as missing.");
			}
			else
			{
				ImGui::TextColored({ 0.9f, 0.6f, 0.3f, 1.0f }, "Only empty folders can be deleted.");
			}

			ImGui::Spacing();
			ImGui::BeginDisabled(canDelete == false);
			if (ImGui::Button("Delete", { 120.0f, 0.0f }))
			{
				// The manager deletes the file and its .hymeta, keeps the handle marked missing (so scenes that use it
				// report it) and logs why when it can't.
				if (AssetManager::DeleteAsset(m_PendingDeletePath))
				{
					HY_INFO("Deleted {}", m_PendingDeletePath.u8string());
					if (OnDeleted)
					{
						OnDeleted(m_PendingDeletePath);
					}
				}

				m_PendingDeletePath.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndDisabled();

			ImGui::SameLine();
			if (ImGui::Button("Cancel", { 120.0f, 0.0f }))
			{
				m_PendingDeletePath.clear();
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void AssetFileActions::DrawNamePopup()
	{
		const char* popupId = "Name";
		if (m_RequestNamePopup)
		{
			ImGui::OpenPopup(popupId);
			m_RequestNamePopup = false;
		}

		if (ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			const bool renaming = m_RenamePath.empty() == false;
			const bool renamingFile = renaming && m_RenameExtension.empty() == false;
			const std::filesystem::path parent = renaming ? m_RenamePath.parent_path() : m_CreateFolder;

			ImGui::TextUnformatted(renaming == false ? "New folder" : (renamingFile ? "Rename asset" : "Rename folder"));

			// Focus the field (with its text selected) when the popup opens.
			if (ImGui::IsWindowAppearing())
			{
				ImGui::SetKeyboardFocusHere();
			}
			ImGui::SetNextItemWidth(300.0f);
			const bool entered = ImGui::InputText("##Name", m_NameBuffer, sizeof(m_NameBuffer),
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
			if (renamingFile)
			{
				ImGui::SameLine();
				ImGui::TextDisabled("%s", m_RenameExtension.c_str());
			}

			const std::string name = m_NameBuffer;
			const std::string problem = ValidateName(name, m_RenameExtension, parent, m_RenamePath);
			if (problem.empty() == false)
			{
				ImGui::TextColored({ 0.9f, 0.6f, 0.3f, 1.0f }, "%s", problem.c_str());
			}
			else if (renaming && renamingFile == false)
			{
				// Sprites and animators reference assets by handle, so they follow a rename.
				ImGui::TextDisabled("Sprites in saved scenes follow by asset handle. Scenes from before handles follow once re-saved.");
			}

			ImGui::Spacing();
			ImGui::BeginDisabled(problem.empty() == false);
			const bool confirm = ImGui::Button(renaming ? "Rename" : "Create", { 120.0f, 0.0f }) || (entered && problem.empty());
			ImGui::EndDisabled();

			ImGui::SameLine();
			const bool cancel = ImGui::Button("Cancel", { 120.0f, 0.0f }) || ImGui::IsKeyPressed(ImGuiKey_Escape, false);

			if (confirm && problem.empty())
			{
				const std::filesystem::path target = parent / std::filesystem::u8path(name + m_RenameExtension);
				if (renaming)
				{
					// The manager moves the file with its .hymeta and keeps the handle, so references follow; it logs why
					// when it can't.
					if (AssetManager::MoveAsset(m_RenamePath, target) && OnRenamed)
					{
						OnRenamed(m_RenamePath, target);
					}
				}
				else
				{
					std::error_code ec;
					if (std::filesystem::create_directory(target, ec) == false)
					{
						HY_ERROR("Could not create {}: {}", target.u8string(), ec.message());
					}
					else if (OnCreated)
					{
						OnCreated(target);
					}
				}

				m_RenamePath.clear();
				m_RenameExtension.clear();
				ImGui::CloseCurrentPopup();
			}
			else if (cancel)
			{
				m_RenamePath.clear();
				m_RenameExtension.clear();
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	std::filesystem::path AssetFileActions::CreateSpriteAnimation(const std::filesystem::path& imagePath)
	{
		const Ref<Texture2D> texture = Texture2D::LoadFromFile(imagePath);
		if (texture == nullptr)
		{
			return {};
		}

		const std::filesystem::path target = GetFreePath(imagePath.parent_path(), imagePath.stem().u8string(),
			SpriteAnimationSerializer::FileExtension);

		SpriteAnimationSetSpecification specification;
		specification.Texture = texture;
		specification.Clips = SpriteAnimationWindow::MakeClipsFromSpriteNames(*texture, 10.0f);
		const SpriteAnimationSet set(specification);
		if (SpriteAnimationSerializer::Serialize(target, set) == false)
		{
			return {};
		}

		HY_INFO("Created {} ({} clips)", target.filename().u8string(), set.GetClipCount());
		return target;
	}

	std::filesystem::path AssetFileActions::CreateMaterial(const std::filesystem::path& folder)
	{
		const std::filesystem::path target = GetFreePath(folder, "New Material", MaterialSerializer::FileExtension);
		if (MaterialSerializer::Serialize(target, MaterialData{}) == false)
		{
			return {};
		}

		HY_INFO("Created {}", target.filename().u8string());
		return target;
	}

	std::filesystem::path AssetFileActions::CreateRenderProfile(const std::filesystem::path& folder)
	{
		const std::filesystem::path target = GetFreePath(folder, "New Render Profile", RenderProfileSerializer::FileExtension);

		// A starting point that shows something: its own environment (the defaults), default post-processing.
		RenderProfileData data;
		data.Environment.Override = true;
		if (RenderProfileSerializer::Serialize(target, data) == false)
		{
			return {};
		}

		HY_INFO("Created {}", target.filename().u8string());
		return target;
	}

	std::filesystem::path AssetFileActions::CreateDataAsset(const std::filesystem::path& folder, const DataTypeInfo& type, bool table)
	{
		const std::string base = "New " + type.DisplayName + (table ? " Table" : "");
		const std::filesystem::path target = GetFreePath(folder, base,
			table ? DataTableAsset::FileExtension : DefinitionAsset::FileExtension);

		// The handle first (registered now), then the file with it on its first line, like a new scene.
		const AssetHandle handle = AssetManager::ImportAsset(target);
		bool written = false;
		if (table)
		{
			DataTableData data;
			data.Handle = handle;
			data.RowType = &type;
			written = DataTableAsset::Save(target, data);
		}
		else
		{
			DefinitionData data;
			data.Handle = handle;
			data.Values = DataRecord(type);
			written = DefinitionAsset::Save(target, data);
		}
		if (written == false)
		{
			return {};
		}

		HY_INFO("Created {} ({})", target.filename().u8string(), type.Name);
		return target;
	}
}
