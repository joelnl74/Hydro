#include "TextureImportPanel.h"
#include "PropertyUI.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <Hydro/Renderer/Renderer2D.h>
#include <Hydro/Renderer/SpriteSlicer.h>
#include <Hydro/Renderer/TextureImportSerializer.h>

#include <algorithm>
#include <system_error>

namespace Hydro
{
	static std::filesystem::file_time_type GetImportFileTime(const std::filesystem::path& imagePath)
	{
		std::error_code error;
		const std::filesystem::file_time_type time = std::filesystem::last_write_time(Texture2D::GetImportFilePath(imagePath), error);
		return error ? std::filesystem::file_time_type{} : time;
	}

	void TextureImportPanel::Load(const std::filesystem::path& imagePath)
	{
		m_Path = imagePath;
		m_LoadedImportTime = GetImportFileTime(imagePath);
		m_Texture = Texture2D::LoadFromFile(imagePath);
		m_Config = m_Texture != nullptr ? m_Texture->GetImportConfig() : TextureImportConfig{};
		m_Dirty = false;
	}

	void TextureImportPanel::Apply()
	{
		// A sheet without sprites yet gets them from its grid now, so the file lists them (and the
		// Sprite Editor has something to show).
		if (m_Config.Mode == SpriteMode::Multiple && m_Config.Sprites.empty() && m_Config.Slicing.Mode != SpriteSlicingMode::Manual && m_Texture != nullptr)
		{
			if (m_Config.Slicing.Mode == SpriteSlicingMode::Auto)
			{
				SpriteSlicer::DetectGrid(m_Path, m_Config, m_Config.Slicing);
			}
			m_Config.Sprites = SpriteSlicer::SliceGrid(m_Texture->GetWidth(), m_Texture->GetHeight(), m_Config.Slicing, m_Path.stem().u8string());
		}

		if (TextureImportSerializer::Serialize(Texture2D::GetImportFilePath(m_Path), m_Config) == false)
		{
			return;
		}

		const std::filesystem::path path = m_Path;
		Load(path);
		if (OnReimported)
		{
			OnReimported(path);
		}
	}

	void TextureImportPanel::OnImguiRender(const std::filesystem::path& imagePath)
	{
		// Another image, or the file changed elsewhere (the Sprite Editor saved it) while nothing here
		// was edited: read it again.
		if (imagePath != m_Path || (m_Dirty == false && GetImportFileTime(imagePath) != m_LoadedImportTime))
		{
			Load(imagePath);
		}

		ImGui::TextUnformatted(m_Path.filename().u8string().c_str());
		if (m_Texture == nullptr)
		{
			ImGui::TextDisabled("This image can't be loaded (see the Console).");
			return;
		}
		ImGui::TextDisabled("%u x %u px, %u sprite%s", m_Texture->GetWidth(), m_Texture->GetHeight(),
			m_Texture->GetSpriteCount(), m_Texture->GetSpriteCount() == 1 ? "" : "s");

		// Preview: the whole image, fitted to the panel.
		{
			const float available = ImGui::GetContentRegionAvail().x;
			const float boxSize = std::min(available, 256.0f);
			const float width = (float)std::max(m_Texture->GetWidth(), 1u);
			const float height = (float)std::max(m_Texture->GetHeight(), 1u);
			const float scale = boxSize / std::max(width, height);
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (available - width * scale) * 0.5f);
			ImGui::Image((ImTextureID)Renderer2D::GetImGuiTexture(m_Texture), { width * scale, height * scale });
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextDisabled("Import Settings");

		PropertyUI::Row("Sprite Mode", [&]()
		{
			const char* modes[] = { "Single", "Multiple (sheet)" };
			int mode = (int)m_Config.Mode;
			if (ImGui::Combo("##SpriteMode", &mode, modes, 2))
			{
				m_Config.Mode = (SpriteMode)mode;
				m_Dirty = true;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Single: the whole image is one sprite.\nMultiple: a sprite sheet, sliced into named sprites in the Sprite Editor.");
			}
		});

		PropertyUI::Row("Pixels Per Unit", [&]()
		{
			m_Dirty |= PropertyUI::InputFloat("##PixelsPerUnit", m_Config.PixelsPerUnit, "%.1f", 0.01f, 100000.0f);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("How many pixels make one world unit (Set Native Size on a sprite uses it).");
			}
		});

		PropertyUI::Row("Filter", [&]()
		{
			const char* filters[] = { "Linear (smooth)", "Nearest (pixel art)" };
			int filter = m_Config.Properties.SamplerFilter == TextureFilter::Nearest ? 1 : 0;
			if (ImGui::Combo("##Filter", &filter, filters, 2))
			{
				m_Config.Properties.SamplerFilter = filter == 1 ? TextureFilter::Nearest : TextureFilter::Linear;
				m_Dirty = true;
			}
		});

		PropertyUI::Row("Wrap", [&]()
		{
			const char* wraps[] = { "Repeat", "Clamp" };
			int wrap = m_Config.Properties.SamplerWrap == TextureWrap::Clamp ? 1 : 0;
			if (ImGui::Combo("##Wrap", &wrap, wraps, 2))
			{
				m_Config.Properties.SamplerWrap = wrap == 1 ? TextureWrap::Clamp : TextureWrap::Repeat;
				m_Dirty = true;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Repeat lets a sprite tile. Use Clamp for sprite sheets so edges don't bleed.");
			}
		});

		PropertyUI::Row("Colour Space", [&]()
		{
			const char* formats[] = { "sRGB (colour)", "Linear (data, masks)" };
			int format = m_Config.Format == ImageFormat::RGBA ? 1 : 0;
			if (ImGui::Combo("##Format", &format, formats, 2))
			{
				m_Config.Format = format == 1 ? ImageFormat::RGBA : ImageFormat::SRGB;
				m_Dirty = true;
			}
		});

		PropertyUI::Row("Colour Key", [&]()
		{
			m_Dirty |= ImGui::Checkbox("##UseColorKey", &m_Config.UseColorKey);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Make one exact colour transparent (sheets exported without alpha).");
			}
			ImGui::SameLine();
			ImGui::BeginDisabled(m_Config.UseColorKey == false);
			float colour[3] = { m_Config.ColorKey.r / 255.0f, m_Config.ColorKey.g / 255.0f, m_Config.ColorKey.b / 255.0f };
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::ColorEdit3("##ColorKey", colour, ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_DisplayRGB))
			{
				m_Config.ColorKey = glm::uvec3(glm::round(glm::vec3(colour[0], colour[1], colour[2]) * 255.0f));
				m_Dirty = true;
			}
			ImGui::EndDisabled();
		});

		if (m_Config.Mode == SpriteMode::Multiple)
		{
			ImGui::Spacing();
			const size_t spriteCount = m_Config.Sprites.size();
			if (spriteCount == 0)
			{
				ImGui::TextWrapped("No sprites yet: Apply slices the image with the current grid, or slice it in the Sprite Editor.");
			}
			else
			{
				ImGui::TextDisabled("%zu sprites (%s slicing)", spriteCount,
					m_Config.Slicing.Mode == SpriteSlicingMode::Manual ? "manual" : (m_Config.Slicing.Mode == SpriteSlicingMode::Auto ? "auto" : "grid"));
			}

			ImGui::BeginDisabled(m_Dirty);
			if (ImGui::Button("Open Sprite Editor", { ImGui::GetContentRegionAvail().x, 0.0f }) && OnOpenSpriteEditor)
			{
				OnOpenSpriteEditor(m_Path);
			}
			ImGui::EndDisabled();
			if (m_Dirty && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			{
				ImGui::SetTooltip("Apply or Revert the import settings first.");
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
		ImGui::BeginDisabled(m_Dirty == false);
		if (ImGui::Button("Revert", { half, 0.0f }))
		{
			Load(m_Path);
		}
		ImGui::SameLine();
		if (ImGui::Button("Apply", { half, 0.0f }))
		{
			Apply();
		}
		ImGui::EndDisabled();
		if (m_Dirty)
		{
			ImGui::TextDisabled("Unapplied changes. Apply re-imports the image.");
		}
	}
}
