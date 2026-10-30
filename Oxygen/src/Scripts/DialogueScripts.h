#pragma once
#include "Hydro.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// A dialogue box on the runtime UI (UI5): a speaker's lines typed in one by one. Field NPCs open it through the
// MVP director (MvpScripts.h, E near an "Npc <Name>"); the lines are in code until there is a dialogue asset.
namespace Hydro
{
	// On "Dialogue Canvas" (hidden, captures input, no buttons): its children "Dialogue Name" (text), "Dialogue
	// Text" (text, wrapped) and "Dialogue Next" (the blinking arrow). Open() shows it; while it is shown the player
	// can't walk or interact (UI::CapturesInput). Submit (Enter / Space / Z, sent to the canvas since nothing is
	// focused) shows the whole line when it is still typing, else the next line; after the last it closes. Cancel
	// (Esc / Backspace / X) closes it at once.
	HY_SCRIPT()
	class DialogueScript : public ScriptableEntity
	{
	public:
		HY_PROPERTY(Range(1, 200), Tooltip("How fast the lines type in, in characters per second"))
		float CharactersPerSecond = 40.0f;

		void Open(const std::string& speaker, const std::vector<std::string>& lines)
		{
			if (lines.empty())
			{
				return;
			}
			Scene& scene = *GetEntity().GetScene();
			m_Lines = lines;
			m_Line = 0;
			UI::SetText(Child(scene, "Dialogue Name"), speaker);
			StartLine(scene);
			UI::SetVisible(GetEntity(), true);
		}

		bool IsOpen() { return UI::IsVisible(GetEntity()); }

	protected:
		void OnCreate() override
		{
			UI::SetVisible(GetEntity(), false);
		}

		void OnUpdate(float deltaTime) override
		{
			if (IsOpen() == false || m_Line >= (int)m_Lines.size())
			{
				return;
			}
			Scene& scene = *GetEntity().GetScene();
			m_Typed = std::min(m_Typed + CharactersPerSecond * deltaTime, (float)m_Length);
			const int shown = (int)m_Typed;
			Entity text = Child(scene, "Dialogue Text");
			if (text.IsValid() && text.HasComponent<UITextComponent>())
			{
				// Typed in by count: the line is laid out in full from the start, so nothing re-wraps as it grows.
				text.GetComponent<UITextComponent>().VisibleCharacters = shown >= m_Length ? -1 : shown;
			}
			// The arrow blinks once the line is out.
			m_Blink += deltaTime;
			UI::SetVisible(Child(scene, "Dialogue Next"), shown >= m_Length && std::fmod(m_Blink, 0.8f) < 0.5f);
		}

		bool OnUIEvent(const UIEvent& event) override
		{
			if (event.Target != GetEntity())
			{
				return false;
			}
			Scene& scene = *GetEntity().GetScene();
			if (event.Type == UIEventType::Cancel)
			{
				UI::SetVisible(GetEntity(), false);
				return true;
			}
			if (event.Type != UIEventType::Submit)
			{
				return false;
			}
			if ((int)m_Typed < m_Length)
			{
				m_Typed = (float)m_Length; // finish the line
				return true;
			}
			m_Line++;
			if (m_Line >= (int)m_Lines.size())
			{
				UI::SetVisible(GetEntity(), false);
				return true;
			}
			StartLine(scene);
			return true;
		}

	private:
		void StartLine(Scene& scene)
		{
			const std::string& line = m_Lines[(size_t)m_Line];
			UI::SetText(Child(scene, "Dialogue Text"), line);
			m_Length = CountVisible(line);
			m_Typed = 0.0f;
			m_Blink = 0.0f;
			Entity text = Child(scene, "Dialogue Text");
			if (text.IsValid() && text.HasComponent<UITextComponent>())
			{
				text.GetComponent<UITextComponent>().VisibleCharacters = 0;
			}
			UI::SetVisible(Child(scene, "Dialogue Next"), false);
		}

		// What UITextComponent::VisibleCharacters counts: characters (UTF-8 code points), not spaces or line breaks.
		static int CountVisible(const std::string& text)
		{
			int count = 0;
			for (const char c : text)
			{
				const unsigned char byte = (unsigned char)c;
				if ((byte & 0xC0) != 0x80 && byte != ' ' && byte != '\n')
				{
					count++;
				}
			}
			return count;
		}

		// An entity in this canvas's subtree by its name.
		Entity Child(Scene& scene, const std::string& name)
		{
			std::vector<Entity> subtree;
			scene.GetSubtree(GetEntity(), subtree);
			for (const Entity& entity : subtree)
			{
				if (entity.GetName() == name)
				{
					return entity;
				}
			}
			return {};
		}

		std::vector<std::string> m_Lines;
		int m_Line = 0;
		int m_Length = 0;     // the current line's characters (as VisibleCharacters counts them)
		float m_Typed = 0.0f; // how many are shown
		float m_Blink = 0.0f;
	};
}
