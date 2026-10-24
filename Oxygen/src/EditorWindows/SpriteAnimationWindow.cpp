#include "SpriteAnimationWindow.h"
#include "PropertyUI.h"
#include "Utilities/AssetPicker.h"
#include "Utilities/PathFollower.h"

#include <imgui/imgui.h>

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Core/Log.h>

#include <Hydro/Core/Time.h>
#include <Hydro/Scene/SpriteAnimationSerializer.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <unordered_set>

namespace Hydro
{
	// "Farmer_Left_3" -> "Farmer_Left"; names without a trailing "_<number>" are their own group.
	static std::string GetSpriteGroup(const std::string& name)
	{
		const size_t underscore = name.find_last_of('_');
		if (underscore == std::string::npos || underscore + 1 >= name.size())
		{
			return name;
		}
		const bool numbered = std::all_of(name.begin() + (std::ptrdiff_t)underscore + 1, name.end(), [](char c) { return std::isdigit((unsigned char)c) != 0; });
		return numbered ? name.substr(0, underscore) : name;
	}

	static bool ContainsIgnoringCase(const std::string& text, const char* part)
	{
		if (part[0] == '\0')
		{
			return true;
		}
		std::string lowerText = text;
		std::string lowerPart = part;
		std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), [](unsigned char c) { return (char)std::tolower(c); });
		std::transform(lowerPart.begin(), lowerPart.end(), lowerPart.begin(), [](unsigned char c) { return (char)std::tolower(c); });
		return lowerText.find(lowerPart) != std::string::npos;
	}

	std::vector<SpriteAnimationClip> SpriteAnimationWindow::MakeClipsFromSpriteNames(const Texture2D& texture, float frameRate)
	{
		const float duration = 1.0f / std::max(frameRate, 0.001f);
		std::vector<SpriteAnimationClip> clips;
		std::map<std::string, size_t> clipByGroup;

		for (uint32_t i = 0; i < texture.GetSpriteCount(); i++)
		{
			const std::string group = GetSpriteGroup(texture.GetSprite(i).Name);
			auto found = clipByGroup.find(group);
			if (found == clipByGroup.end())
			{
				SpriteAnimationClip clip;
				clip.Name = group;
				clip.Loop = AnimationLoopMode::Loop;
				found = clipByGroup.emplace(group, clips.size()).first;
				clips.push_back(std::move(clip));
			}
			clips[found->second].Frames.push_back({ i, duration });
		}
		return clips;
	}

	uint32_t SpriteAnimationWindow::GetFrameAtTime(const SpriteAnimationClip& clip, float time)
	{
		const uint32_t frameCount = (uint32_t)clip.Frames.size();
		if (frameCount <= 1)
		{
			return 0;
		}

		// The order frames are shown in during one cycle.
		std::vector<uint32_t> order;
		for (uint32_t i = 0; i < frameCount; i++)
		{
			order.push_back(i);
		}
		if (clip.Loop == AnimationLoopMode::PingPong)
		{
			for (uint32_t i = frameCount - 2; i > 0; i--)
			{
				order.push_back(i);
			}
		}

		float cycle = 0.0f;
		for (uint32_t frame : order)
		{
			cycle += std::max(clip.Frames[frame].Duration, SpriteAnimationClip::MinFrameDuration);
		}

		float t = std::max(time, 0.0f);
		if (clip.Loop == AnimationLoopMode::Once)
		{
			if (t >= cycle)
			{
				return frameCount - 1;
			}
		}
		else
		{
			t = std::fmod(t, cycle);
		}

		for (uint32_t frame : order)
		{
			t -= std::max(clip.Frames[frame].Duration, SpriteAnimationClip::MinFrameDuration);
			if (t < 0.0f)
			{
				return frame;
			}
		}
		return order.back();
	}

	void SpriteAnimationWindow::Open(const std::filesystem::path& path)
	{
		m_Open = true;
		ImGui::SetWindowFocus("Animation");
		if (path == m_Path && m_Loaded)
		{
			return;
		}

		if (m_Dirty)
		{
			m_PendingOpen = path;
			m_RequestDiscardPopup = true;
			return;
		}
		Load(path);
	}

	void SpriteAnimationWindow::OnAssetMoved(const std::filesystem::path& from, const std::filesystem::path& to)
	{
		PathFollower::Follow(m_Path, from, to);
		PathFollower::Follow(m_PendingOpen, from, to);
	}

	void SpriteAnimationWindow::OnAssetDeleted(const std::filesystem::path& path)
	{
		if (PathFollower::Follow(m_Path, path, path) == false)
		{
			return;
		}

		m_Open = false;
		m_Path.clear();
		m_Loaded = false;
		m_Texture = nullptr;
		m_Clips.clear();
		m_SelectedClip = -1;
		m_SelectedFrame = -1;
		m_Dirty = false;
		m_PendingOpen.clear();
	}

	void SpriteAnimationWindow::Load(const std::filesystem::path& path)
	{
		m_Path = path;
		SpriteAnimationSetSpecification specification;
		m_Loaded = SpriteAnimationSerializer::Deserialize(path, specification);
		m_Texture = specification.Texture;
		m_Clips = specification.Clips;
		m_DefaultClip = (int32_t)specification.DefaultClip;
		m_SelectedClip = m_Clips.empty() ? -1 : m_DefaultClip;
		m_SelectedFrame = -1;
		m_PreviewTime = 0.0f;
		m_Dirty = false;
	}

	void SpriteAnimationWindow::Save()
	{
		if (m_Texture == nullptr)
		{
			HY_ERROR("Animation: pick a sprite sheet before saving {}", m_Path.filename().u8string());
			return;
		}

		std::unordered_set<std::string> names;
		for (const SpriteAnimationClip& clip : m_Clips)
		{
			if (names.insert(clip.Name).second == false)
			{
				HY_WARN("Animation: two clips are named '{}'; Play finds the first", clip.Name);
			}
		}

		SpriteAnimationSetSpecification specification;
		specification.Texture = m_Texture;
		specification.Clips = m_Clips;
		specification.DefaultClip = (uint32_t)std::max(m_DefaultClip, 0);
		const SpriteAnimationSet set(specification);
		if (SpriteAnimationSerializer::Serialize(m_Path, set) == false)
		{
			return;
		}

		m_Dirty = false;
		if (OnSaved)
		{
			OnSaved(m_Path);
		}
	}

	void SpriteAnimationWindow::SetTexture(const Ref<Texture2D>& texture)
	{
		if (texture == nullptr || texture == m_Texture)
		{
			return;
		}

		// Keep frames that name a sprite the new sheet also has; frames are indices, so map them by name.
		for (SpriteAnimationClip& clip : m_Clips)
		{
			std::vector<SpriteAnimationFrame> kept;
			for (const SpriteAnimationFrame& frame : clip.Frames)
			{
				const int32_t sprite = m_Texture != nullptr ? texture->FindSprite(m_Texture->GetSprite(frame.Sprite).Name) : -1;
				if (sprite >= 0)
				{
					kept.push_back({ (uint32_t)sprite, frame.Duration });
				}
			}
			clip.Frames = std::move(kept);
		}

		m_Texture = texture;
		m_Loaded = true;
		m_Dirty = true;
		if (texture->GetSpriteMode() == SpriteMode::Single)
		{
			HY_WARN("Animation: {} is a Single texture (one sprite). Set its Sprite Mode to Multiple to animate it.", texture->GetPath().filename().u8string());
		}
	}

	SpriteAnimationClip* SpriteAnimationWindow::GetSelectedClip()
	{
		if (m_SelectedClip < 0 || m_SelectedClip >= (int32_t)m_Clips.size())
		{
			return nullptr;
		}
		return &m_Clips[m_SelectedClip];
	}

	void SpriteAnimationWindow::OnImguiRender()
	{
		DrawDiscardPopup();
		if (m_Open == false)
		{
			return;
		}

		ImGui::SetNextWindowSize({ 1000.0f, 620.0f }, ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Animation", &m_Open, m_Dirty ? ImGuiWindowFlags_UnsavedDocument : 0) == false)
		{
			ImGui::End();
			return;
		}

		if (m_Path.empty())
		{
			ImGui::TextDisabled("Double-click a .hyanim in the Project window, or right-click a sprite sheet > Create Sprite Animation.");
			ImGui::End();
			return;
		}

		if (m_PreviewPlaying)
		{
			m_PreviewTime += Time::GetDeltaTime() * m_PreviewSpeed;
		}

		DrawToolbar();
		ImGui::Separator();

		if (ImGui::BeginTable("##AnimationLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
		{
			ImGui::TableSetupColumn("Clips", ImGuiTableColumnFlags_WidthFixed, 220.0f);
			ImGui::TableSetupColumn("Clip", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed, 300.0f);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			if (ImGui::BeginChild("##ClipList"))
			{
				DrawClipList();
			}
			ImGui::EndChild();

			ImGui::TableSetColumnIndex(1);
			if (ImGui::BeginChild("##Clip"))
			{
				DrawClip();
			}
			ImGui::EndChild();

			ImGui::TableSetColumnIndex(2);
			if (ImGui::BeginChild("##PreviewColumn"))
			{
				DrawPreview();
				ImGui::Spacing();
				DrawSpritePicker();
			}
			ImGui::EndChild();

			ImGui::EndTable();
		}

		ImGui::End();
	}

	void SpriteAnimationWindow::DrawToolbar()
	{
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(m_Path.filename().u8string().c_str());
		ImGui::SameLine();
		ImGui::TextDisabled("Sheet:");
		ImGui::SameLine();

		// The sprite sheet: pick one of the project's images, or drop one from the Project window.
		// Frames keep the sprites the new sheet has by name. (Clearing it isn't an option: a set needs a sheet.)
		const AssetHandle current = m_Texture != nullptr ? m_Texture->GetHandle() : AssetHandle(0);
		const AssetPickerResult result = AssetPicker::DrawField("##Sheet", current, AssetType::Texture2D, "Frames keep sprites the new sheet has by name.", 240.0f);
		if (result.Changed && static_cast<uint64_t>(result.Handle) != 0)
		{
			SetTexture(AssetManager::GetAsset<Texture2D>(result.Handle));
		}

		ImGui::SameLine();
		ImGui::TextDisabled("Default clip:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(160.0f);
		const char* defaultName = m_DefaultClip >= 0 && m_DefaultClip < (int32_t)m_Clips.size() ? m_Clips[m_DefaultClip].Name.c_str() : "None";
		if (ImGui::BeginCombo("##DefaultClip", defaultName))
		{
			for (int32_t i = 0; i < (int32_t)m_Clips.size(); i++)
			{
				ImGui::PushID(i);
				if (ImGui::Selectable(m_Clips[i].Name.c_str(), i == m_DefaultClip))
				{
					m_DefaultClip = i;
					m_Dirty = true;
				}
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Played by a Sprite Animator whose Start Clip is empty.");
		}

		const float buttonWidth = 90.0f;
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 2.0f * buttonWidth - ImGui::GetStyle().ItemSpacing.x);
		ImGui::BeginDisabled(m_Dirty == false);
		if (ImGui::Button("Revert", { buttonWidth, 0.0f }))
		{
			Load(m_Path);
		}
		ImGui::SameLine();
		if (ImGui::Button("Save", { buttonWidth, 0.0f }))
		{
			Save();
		}
		ImGui::EndDisabled();

		if (m_Loaded == false && m_Texture == nullptr)
		{
			ImGui::TextDisabled("The file or its sheet couldn't be read (see the Console). Pick a sheet to start over.");
		}
	}

	void SpriteAnimationWindow::DrawClipList()
	{
		ImGui::TextDisabled("Clips");
		const float listHeight = std::max(ImGui::GetContentRegionAvail().y - 3.0f * ImGui::GetFrameHeightWithSpacing(), 80.0f);
		if (ImGui::BeginListBox("##Clips", { -1.0f, listHeight }))
		{
			for (int32_t i = 0; i < (int32_t)m_Clips.size(); i++)
			{
				ImGui::PushID(i);
				char label[160];
				std::snprintf(label, sizeof(label), "%s%s", m_Clips[i].Name.c_str(), i == m_DefaultClip ? "  (default)" : "");
				if (ImGui::Selectable(label, i == m_SelectedClip))
				{
					m_SelectedClip = i;
					m_SelectedFrame = -1;
					m_PreviewTime = 0.0f;
				}
				ImGui::PopID();
			}
			ImGui::EndListBox();
		}

		const float third = (ImGui::GetContentRegionAvail().x - 2.0f * ImGui::GetStyle().ItemSpacing.x) / 3.0f;
		if (ImGui::Button("Add", { third, 0.0f }))
		{
			SpriteAnimationClip clip;
			clip.Name = "Clip" + std::to_string(m_Clips.size());
			m_Clips.push_back(clip);
			m_SelectedClip = (int32_t)m_Clips.size() - 1;
			m_Dirty = true;
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(GetSelectedClip() == nullptr);
		if (ImGui::Button("Copy", { third, 0.0f }))
		{
			SpriteAnimationClip copy = *GetSelectedClip();
			copy.Name += "_Copy";
			m_Clips.insert(m_Clips.begin() + m_SelectedClip + 1, copy);
			m_SelectedClip++;
			m_DefaultClip += m_DefaultClip >= m_SelectedClip ? 1 : 0;
			m_Dirty = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Delete", { third, 0.0f }))
		{
			m_Clips.erase(m_Clips.begin() + m_SelectedClip);
			if (m_DefaultClip > m_SelectedClip || m_DefaultClip >= (int32_t)m_Clips.size())
			{
				m_DefaultClip = std::max(m_DefaultClip - 1, 0);
			}
			m_SelectedClip = std::min(m_SelectedClip, (int32_t)m_Clips.size() - 1);
			m_Dirty = true;
		}
		ImGui::EndDisabled();

		ImGui::BeginDisabled(m_Texture == nullptr);
		if (ImGui::Button("Clips from Sprite Names", { -1.0f, 0.0f }))
		{
			// Only clips that don't exist yet, so renamed or edited ones stay as they are.
			size_t added = 0;
			for (SpriteAnimationClip& clip : MakeClipsFromSpriteNames(*m_Texture, m_FrameRateForAll))
			{
				const bool exists = std::any_of(m_Clips.begin(), m_Clips.end(), [&](const SpriteAnimationClip& other) { return other.Name == clip.Name; });
				if (exists == false)
				{
					m_Clips.push_back(std::move(clip));
					added++;
				}
			}
			m_Dirty |= added > 0;
			if (m_SelectedClip < 0 && m_Clips.empty() == false)
			{
				m_SelectedClip = 0;
			}
		}
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("One clip per group of sprites named <name>_<number> (e.g. one per named row of the sheet).\nClips that already exist are left alone. Rename them after, e.g. Farmer_Left -> WalkLeft.");
		}
	}

	void SpriteAnimationWindow::DrawClip()
	{
		SpriteAnimationClip* clip = GetSelectedClip();
		if (clip == nullptr)
		{
			ImGui::TextDisabled("Add a clip, or select one on the left.");
			return;
		}

		PropertyUI::Row("Name", [&]()
		{
			char name[128];
			std::snprintf(name, sizeof(name), "%s", clip->Name.c_str());
			if (ImGui::InputText("##Name", name, sizeof(name)))
			{
				clip->Name = name;
				m_Dirty = true;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("What SpriteAnimator::Play takes, e.g. Play(entity, \"WalkLeft\").");
			}
		});

		PropertyUI::Row("Loop", [&]()
		{
			const char* modes[] = { "Once", "Loop", "Ping Pong" };
			int mode = (int)clip->Loop;
			if (ImGui::Combo("##Loop", &mode, modes, 3))
			{
				clip->Loop = (AnimationLoopMode)mode;
				m_Dirty = true;
			}
		});

		PropertyUI::Row("Flip X", [&]()
		{
			m_Dirty |= ImGui::Checkbox("##FlipX", &clip->FlipX);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Mirror the frames, e.g. a WalkLeft clip made from right-facing sprites.");
			}
		});

		PropertyUI::Row("Frame Rate", [&]()
		{
			const float applyWidth = 90.0f;
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - applyWidth - ImGui::GetStyle().ItemSpacing.x);
			PropertyUI::InputFloat("##FrameRate", m_FrameRateForAll, "%.1f fps", 0.1f, 240.0f);
			ImGui::SameLine();
			if (ImGui::Button("Set All", { applyWidth, 0.0f }))
			{
				for (SpriteAnimationFrame& frame : clip->Frames)
				{
					frame.Duration = 1.0f / m_FrameRateForAll;
				}
				m_Dirty = true;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Every frame of this clip lasts 1 / frame rate seconds.\nAlso the rate for frames added from the sprite picker and Clips from Sprite Names.");
			}
		});

		ImGui::TextDisabled("%zu frames, %.2f s", clip->Frames.size(), clip->GetLength());
		ImGui::Spacing();
		DrawFrames(*clip);
		ImGui::Spacing();
		DrawEvents(*clip);
	}

	void SpriteAnimationWindow::DrawFrames(SpriteAnimationClip& clip)
	{
		ImGui::TextDisabled("Frames (click a sprite on the right to add one)");
		const uint32_t previewFrame = GetFrameAtTime(clip, m_PreviewTime);

		int32_t moveFrom = -1;
		int32_t moveTo = -1;
		int32_t removeFrame = -1;
		const float thumbnail = 40.0f;
		if (ImGui::BeginTable("##Frames", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("##Image", ImGuiTableColumnFlags_WidthFixed, thumbnail);
			ImGui::TableSetupColumn("Sprite", ImGuiTableColumnFlags_WidthStretch, 2.0f);
			ImGui::TableSetupColumn("Seconds", ImGuiTableColumnFlags_WidthStretch, 1.0f);
			ImGui::TableSetupColumn("##Actions", ImGuiTableColumnFlags_WidthFixed, 3.0f * ImGui::GetFrameHeight() + 2.0f * ImGui::GetStyle().ItemSpacing.x);
			ImGui::TableHeadersRow();

			for (int32_t i = 0; i < (int32_t)clip.Frames.size(); i++)
			{
				SpriteAnimationFrame& frame = clip.Frames[i];
				ImGui::PushID(i);
				ImGui::TableNextRow();
				if ((uint32_t)i == previewFrame)
				{
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_Header));
				}

				ImGui::TableSetColumnIndex(0);
				PropertyUI::SpriteImage(m_Texture, frame.Sprite, thumbnail, clip.FlipX);
				if (ImGui::IsItemClicked())
				{
					m_SelectedFrame = i;
				}

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-1.0f);
				const char* spriteName = m_Texture != nullptr ? m_Texture->GetSprite(frame.Sprite).Name.c_str() : "?";
				if (ImGui::BeginCombo("##Sprite", spriteName, ImGuiComboFlags_HeightLarge))
				{
					for (uint32_t s = 0; m_Texture != nullptr && s < m_Texture->GetSpriteCount(); s++)
					{
						ImGui::PushID((int)s);
						if (ImGui::Selectable(m_Texture->GetSprite(s).Name.c_str(), s == frame.Sprite))
						{
							frame.Sprite = s;
							m_Dirty = true;
						}
						ImGui::PopID();
					}
					ImGui::EndCombo();
				}

				ImGui::TableSetColumnIndex(2);
				ImGui::SetNextItemWidth(-1.0f);
				m_Dirty |= PropertyUI::InputFloat("##Duration", frame.Duration, "%.3f", SpriteAnimationClip::MinFrameDuration, 60.0f);

				ImGui::TableSetColumnIndex(3);
				const ImVec2 button = { ImGui::GetFrameHeight(), ImGui::GetFrameHeight() };
				ImGui::BeginDisabled(i == 0);
				if (ImGui::ArrowButton("##Up", ImGuiDir_Up))
				{
					moveFrom = i;
					moveTo = i - 1;
				}
				ImGui::EndDisabled();
				ImGui::SameLine();
				ImGui::BeginDisabled(i + 1 == (int32_t)clip.Frames.size());
				if (ImGui::ArrowButton("##Down", ImGuiDir_Down))
				{
					moveFrom = i;
					moveTo = i + 1;
				}
				ImGui::EndDisabled();
				ImGui::SameLine();
				if (ImGui::Button("X", button))
				{
					removeFrame = i;
				}

				ImGui::PopID();
			}
			ImGui::EndTable();
		}

		// Changes after the loop, so the list isn't changed while it is drawn.
		if (moveFrom >= 0)
		{
			std::swap(clip.Frames[moveFrom], clip.Frames[moveTo]);
			m_SelectedFrame = moveTo;
			m_Dirty = true;
		}
		if (removeFrame >= 0)
		{
			clip.Frames.erase(clip.Frames.begin() + removeFrame);
			for (SpriteAnimationEvent& event : clip.Events)
			{
				if (event.Frame > (uint32_t)removeFrame)
				{
					event.Frame--;
				}
			}
			m_SelectedFrame = std::min(m_SelectedFrame, (int32_t)clip.Frames.size() - 1);
			m_Dirty = true;
		}
	}

	void SpriteAnimationWindow::DrawEvents(SpriteAnimationClip& clip)
	{
		ImGui::TextDisabled("Events (saved; scripts can't receive them yet)");

		int32_t removeEvent = -1;
		for (int32_t i = 0; i < (int32_t)clip.Events.size(); i++)
		{
			SpriteAnimationEvent& event = clip.Events[i];
			ImGui::PushID(i);

			ImGui::SetNextItemWidth(90.0f);
			int frame = (int)event.Frame;
			if (ImGui::InputInt("##Frame", &frame))
			{
				event.Frame = (uint32_t)std::clamp(frame, 0, std::max((int)clip.Frames.size() - 1, 0));
				m_Dirty = true;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Frame the event fires on (0 = first).");
			}
			ImGui::SameLine();
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x);
			char name[96];
			std::snprintf(name, sizeof(name), "%s", event.Name.c_str());
			if (ImGui::InputText("##Name", name, sizeof(name)))
			{
				event.Name = name;
				m_Dirty = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("X", { ImGui::GetFrameHeight(), ImGui::GetFrameHeight() }))
			{
				removeEvent = i;
			}

			ImGui::PopID();
		}

		if (removeEvent >= 0)
		{
			clip.Events.erase(clip.Events.begin() + removeEvent);
			m_Dirty = true;
		}

		if (ImGui::Button("Add Event"))
		{
			SpriteAnimationEvent event;
			event.Frame = (uint32_t)std::max(m_SelectedFrame, 0);
			event.Name = "Event";
			clip.Events.push_back(event);
			m_Dirty = true;
		}
	}

	void SpriteAnimationWindow::DrawPreview()
	{
		ImGui::TextDisabled("Preview");
		const SpriteAnimationClip* clip = GetSelectedClip();
		const float size = std::min(ImGui::GetContentRegionAvail().x, 220.0f);

		const ImVec2 boxMin = ImGui::GetCursorScreenPos();
		ImGui::GetWindowDrawList()->AddRectFilled(boxMin, { boxMin.x + size, boxMin.y + size }, ImGui::GetColorU32(ImGuiCol_FrameBg), 4.0f);
		if (clip != nullptr && clip->Frames.empty() == false && m_Texture != nullptr)
		{
			const uint32_t frame = GetFrameAtTime(*clip, m_PreviewTime);
			PropertyUI::SpriteImage(m_Texture, clip->Frames[frame].Sprite, size, clip->FlipX);
			ImGui::TextDisabled("Frame %u / %zu", frame, clip->Frames.size());
		}
		else
		{
			ImGui::Dummy({ size, size });
			ImGui::TextDisabled("No frames");
		}

		if (ImGui::Button(m_PreviewPlaying ? "Pause" : "Play"))
		{
			m_PreviewPlaying = m_PreviewPlaying == false;
		}
		ImGui::SameLine();
		if (ImGui::Button("Restart"))
		{
			m_PreviewTime = 0.0f;
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::SliderFloat("##Speed", &m_PreviewSpeed, 0.1f, 4.0f, "Speed %.1fx");
	}

	void SpriteAnimationWindow::DrawSpritePicker()
	{
		ImGui::TextDisabled("Sheet sprites (click to add a frame)");
		if (m_Texture == nullptr)
		{
			ImGui::TextDisabled("Pick a sprite sheet in the toolbar.");
			return;
		}

		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##Filter", "Filter by name", m_SpriteFilter, sizeof(m_SpriteFilter));

		SpriteAnimationClip* clip = GetSelectedClip();
		const float tile = 56.0f;
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const int columns = std::max(1, (int)((ImGui::GetContentRegionAvail().x + spacing) / (tile + spacing)));

		if (ImGui::BeginChild("##Sprites", { 0.0f, 0.0f }, true))
		{
			int column = 0;
			for (uint32_t i = 0; i < m_Texture->GetSpriteCount(); i++)
			{
				const std::string& name = m_Texture->GetSprite(i).Name;
				if (ContainsIgnoringCase(name, m_SpriteFilter) == false)
				{
					continue;
				}

				ImGui::PushID((int)i);
				if (column > 0)
				{
					ImGui::SameLine();
				}
				const ImVec2 min = ImGui::GetCursorScreenPos();
				PropertyUI::SpriteImage(m_Texture, i, tile);
				ImGui::SetCursorScreenPos(min);
				if (ImGui::InvisibleButton("##Pick", { tile, tile }) && clip != nullptr)
				{
					// After the selected frame, else at the end.
					const float duration = 1.0f / m_FrameRateForAll;
					const int32_t at = m_SelectedFrame >= 0 ? m_SelectedFrame + 1 : (int32_t)clip->Frames.size();
					clip->Frames.insert(clip->Frames.begin() + at, { i, duration });
					m_SelectedFrame = m_SelectedFrame >= 0 ? at : -1;
					m_Dirty = true;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::GetWindowDrawList()->AddRect(min, { min.x + tile, min.y + tile }, ImGui::GetColorU32(ImGuiCol_CheckMark), 3.0f, 0, 2.0f);
					if (clip != nullptr)
					{
						ImGui::SetTooltip("%u: %s\nClick to add to the clip", i, name.c_str());
					}
					else
					{
						ImGui::SetTooltip("%u: %s\nSelect a clip to add frames", i, name.c_str());
					}
				}
				ImGui::PopID();

				column = (column + 1) % columns;
			}
		}
		ImGui::EndChild();
	}

	void SpriteAnimationWindow::DrawDiscardPopup()
	{
		if (m_RequestDiscardPopup)
		{
			ImGui::OpenPopup("Discard animation changes?");
			m_RequestDiscardPopup = false;
		}

		if (ImGui::BeginPopupModal("Discard animation changes?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("'%s' has unsaved changes.", m_Path.filename().u8string().c_str());
			ImGui::Spacing();
			if (ImGui::Button("Save", { 120.0f, 0.0f }))
			{
				Save();
				Load(m_PendingOpen);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Discard", { 120.0f, 0.0f }))
			{
				Load(m_PendingOpen);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", { 120.0f, 0.0f }))
			{
				m_PendingOpen.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}
}
