#pragma once
#include "Hydro.h"
#include "VillageScripts.h"
#include "BattleScripts.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

// The MVP scene's mechanics (docs/DioramaRPG.md, the GDD's first-scene test; the scene is written by
// scripts/diorama_rpg/mvp.py): interact prompts (a sign, a chest, the inn's door, a save point, the map's
// exits, an NPC) and a sample inventory menu that gates the player's input while it is open. Prototype
// code, like BattleScripts.h: one script holds it all and the UI is sprites in the scene, placed relative
// to the camera every frame, until the UI system (U1) and text land.
namespace Hydro
{
	// Rows of DioramaRPG/UI/MvpText.png (192 x 16 cells): keep in step with LABELS in scripts/diorama_rpg/mvp.py.
	enum class MvpLabel : uint32_t
	{
		Inventory, EnterUse, EscClose,
		Potion, HiPotion, Ether, IronSword, HerbBundle, PhoenixFeather, CrystalShard, LeatherBoots, Antidote, AncientKey, CopperOre, OldMap,
		PromptRead, PromptOpen, PromptEnter, PromptSave, PromptLeave, PromptTalk,
		SignLine1, SignLine2, FoundPotion, ChestEmpty, Rested, RestedLine2, NoSaveSlot, RoadGoesOn, ToTheVillage,
		UsedPotion, NotUsableHere, NoneLeft, NpcLine1, NpcLine2, NpcLine3,
		Count
	};

	class MvpDirectorScript : public ScriptableEntity
	{
	protected:
		void OnCreate() override
		{
			Scene& scene = *GetEntity().GetScene();
			m_Camera = Find(scene, "Camera");
			m_Player = Find(scene, "Player");
			m_Fade = Find(scene, "UI Fade");
			m_Prompt.Panel = Find(scene, "UI Prompt Panel");
			m_Prompt.Label = Find(scene, "UI Prompt Label");
			m_Message.Panel = Find(scene, "UI Message Panel");
			m_Message.Line[0] = Find(scene, "UI Message Line 1");
			m_Message.Line[1] = Find(scene, "UI Message Line 2");
			m_Menu.Panel = Find(scene, "UI Menu Panel");
			m_Menu.Title = Find(scene, "UI Menu Title");
			m_Menu.Footer[0] = Find(scene, "UI Menu Footer 1");
			m_Menu.Footer[1] = Find(scene, "UI Menu Footer 2");
			m_Menu.Highlight = Find(scene, "UI Menu Highlight");
			m_Menu.Cursor = Find(scene, "UI Menu Cursor");
			for (int i = 0; i < VisibleRows; i++)
			{
				m_Menu.Row[i] = Find(scene, "UI Menu Row " + std::to_string(i));
				m_Menu.Count[i] = Find(scene, "UI Menu Count " + std::to_string(i));
			}

			// The UI entities are authored around the origin: their positions are offsets from the camera.
			auto remember = [&](entt::entity entity)
			{
				if (Alive(scene, entity))
				{
					const glm::vec3& position = scene.m_Registry.get<TransformComponent>(entity).Translation;
					m_Offsets.push_back({ entity, { position.x, position.y } });
				}
			};
			remember(m_Fade);
			remember(m_Message.Panel); remember(m_Message.Line[0]); remember(m_Message.Line[1]);
			remember(m_Menu.Panel); remember(m_Menu.Title); remember(m_Menu.Footer[0]); remember(m_Menu.Footer[1]);
			remember(m_Menu.Highlight); remember(m_Menu.Cursor);
			for (int i = 0; i < VisibleRows; i++)
			{
				remember(m_Menu.Row[i]);
				remember(m_Menu.Count[i]);
			}
			m_HighlightOffset = OffsetOf(m_Menu.Highlight);
			m_CursorOffset = OffsetOf(m_Menu.Cursor);
			m_RowOffset = OffsetOf(m_Menu.Row[0]);
			m_RowStep = VisibleRows > 1 ? OffsetOf(m_Menu.Row[1]).y - m_RowOffset.y : -0.55f;

			// Everything the player can interact with, by tag prefix.
			auto view = scene.m_Registry.view<TagComponent, TransformComponent>();
			for (auto entity : view)
			{
				const std::string& tag = view.get<TagComponent>(entity).Tag;
				Interactable item;
				item.Entity = entity;
				if (tag.rfind("Sign", 0) == 0) { item.Type = Kind::Sign; }
				else if (tag.rfind("Chest", 0) == 0) { item.Type = Kind::Chest; }
				else if (tag.rfind("Door", 0) == 0) { item.Type = Kind::Door; }
				else if (tag.rfind("Save", 0) == 0) { item.Type = Kind::Save; }
				else if (tag.rfind("Exit", 0) == 0) { item.Type = Kind::Exit; }
				else if (tag.rfind("Npc", 0) == 0) { item.Type = Kind::Npc; }
				else { continue; }
				const glm::vec3& position = view.get<TransformComponent>(entity).Translation;
				item.Position = { position.x, position.y };
				if (scene.m_Registry.has<SpriteRendererComponent>(entity))
				{
					// a sprite's entity sits at its middle; the player talks to its feet
					item.Position.y -= 0.5f * view.get<TransformComponent>(entity).Scale.y;
				}
				m_Interactables.push_back(item);
			}

			// The sample inventory (the GDD's prototype HUD list): twelve rows so the list has to scroll.
			m_Items = {
				{ MvpLabel::Potion, 3, true }, { MvpLabel::HiPotion, 1, true }, { MvpLabel::Ether, 2, true }, { MvpLabel::IronSword, 1, false },
				{ MvpLabel::HerbBundle, 4, false }, { MvpLabel::PhoenixFeather, 1, false }, { MvpLabel::CrystalShard, 2, false },
				{ MvpLabel::LeatherBoots, 1, false }, { MvpLabel::Antidote, 2, false }, { MvpLabel::AncientKey, 1, false },
				{ MvpLabel::CopperOre, 5, false }, { MvpLabel::OldMap, 1, false }
			};

			UIState::MenuOpen = false;
			SetAlpha(scene, m_Fade, 0.0f);
			ShowMenu(false);
			ShowPrompt(false);
			m_Message.Time = 0.0f;
			PresentMessage();
			HY_INFO("MVP: WASD walk, E interact, Tab inventory (Up/Down select, Enter use, Esc close), 1/2/3 day-dusk-night");
		}

		void OnUpdate(float deltaTime) override
		{
			Scene& scene = *GetEntity().GetScene();
			m_Time += deltaTime;
			FollowCamera(scene);

			// a scene change in progress: fade, then go
			if (m_Leaving > 0.0f)
			{
				m_Leaving = std::max(0.0f, m_Leaving - deltaTime);
				SetAlpha(scene, m_Fade, 1.0f - m_Leaving / LeaveTime);
				if (m_Leaving <= 0.0f)
				{
					SceneTransition::Push("StatueVillage");
				}
				return;
			}
			// a rest at the inn: a dip to black and back
			if (m_Resting > 0.0f)
			{
				m_Resting = std::max(0.0f, m_Resting - deltaTime);
				const float k = m_Resting / RestTime;
				SetAlpha(scene, m_Fade, k > 0.5f ? (1.0f - k) * 2.0f : k * 2.0f);
			}

			if (KeyPressed(KeyCode::Tab, m_Previous[0]))
			{
				ShowMenu(UIState::MenuOpen == false);
			}
			if (UIState::MenuOpen)
			{
				UpdateMenu(scene);
			}
			else
			{
				UpdateInteraction(scene);
			}

			if (m_Message.Time > 0.0f)
			{
				m_Message.Time = std::max(0.0f, m_Message.Time - deltaTime);
				PresentMessage();
			}
		}

	private:
		enum class Kind : uint8_t { Sign, Chest, Door, Save, Exit, Npc };

		struct Interactable
		{
			entt::entity Entity = entt::null;
			Kind Type = Kind::Sign;
			glm::vec2 Position{ 0.0f };
			bool Used = false;   // chests open once
		};

		struct Item
		{
			MvpLabel Label = MvpLabel::Potion;
			int Count = 0;
			bool Usable = false;
		};

		static constexpr int VisibleRows = 6;
		static constexpr float InteractRange = 1.7f;
		static constexpr float MessageTime = 3.0f;
		static constexpr float LeaveTime = 0.6f;
		static constexpr float RestTime = 1.2f;

		// ---- Interaction --------------------------------------------------------------------------------

		void UpdateInteraction(Scene& scene)
		{
			const glm::vec2 player = Position(scene, m_Player);
			Interactable* nearest = nullptr;
			float best = InteractRange;
			for (Interactable& item : m_Interactables)
			{
				if (Alive(scene, item.Entity) == false)
				{
					continue;
				}
				const float distance = glm::length(item.Position - player);
				if (distance < best)
				{
					best = distance;
					nearest = &item;
				}
			}

			ShowPrompt(nearest != nullptr);
			if (nearest == nullptr)
			{
				return;
			}
			SetSprite(scene, m_Prompt.Label, (uint32_t)PromptFor(nearest->Type));
			SetPosition(scene, m_Prompt.Panel, player + glm::vec2(0.0f, 2.35f));
			SetPosition(scene, m_Prompt.Label, player + glm::vec2(0.0f, 2.35f));

			if (KeyPressed(KeyCode::E, m_Previous[1]) == false || m_Resting > 0.0f)
			{
				return;
			}
			switch (nearest->Type)
			{
				case Kind::Sign:
					ShowMessage(MvpLabel::SignLine1, MvpLabel::SignLine2);
					break;
				case Kind::Chest:
					if (nearest->Used)
					{
						ShowMessage(MvpLabel::ChestEmpty);
					}
					else
					{
						nearest->Used = true;
						SetSprite(scene, nearest->Entity, 1);   // the open lid
						m_Items[0].Count += 1;
						ShowMessage(MvpLabel::FoundPotion);
						HY_INFO("MVP: found a potion ({} now)", m_Items[0].Count);
					}
					break;
				case Kind::Door:
					BattleState::RestoreParty();
					m_Resting = RestTime;
					ShowMessage(MvpLabel::Rested, MvpLabel::RestedLine2);
					HY_INFO("MVP: the party rests at the inn: HP and MP restored");
					break;
				case Kind::Save:
					// J5 (a save slot) is not written yet: the save point only says so.
					ShowMessage(MvpLabel::NoSaveSlot);
					HY_INFO("MVP: save point: saving is not implemented yet (DioramaRPG.md J5)");
					break;
				case Kind::Exit:
					ShowMessage(MvpLabel::RoadGoesOn, MvpLabel::ToTheVillage);
					m_Leaving = LeaveTime;
					HY_INFO("MVP: leaving for the Statue Village");
					break;
				case Kind::Npc:
					ShowMessage((MvpLabel)((uint32_t)MvpLabel::NpcLine1 + m_NpcLine));
					m_NpcLine = (m_NpcLine + 1) % 3;
					break;
			}
		}

		static MvpLabel PromptFor(Kind kind)
		{
			switch (kind)
			{
				case Kind::Chest: return MvpLabel::PromptOpen;
				case Kind::Door: return MvpLabel::PromptEnter;
				case Kind::Save: return MvpLabel::PromptSave;
				case Kind::Exit: return MvpLabel::PromptLeave;
				case Kind::Npc: return MvpLabel::PromptTalk;
				default: return MvpLabel::PromptRead;
			}
		}

		void ShowPrompt(bool shown)
		{
			Scene& scene = *GetEntity().GetScene();
			SetAlpha(scene, m_Prompt.Panel, shown ? 0.85f : 0.0f);
			SetAlpha(scene, m_Prompt.Label, shown ? 1.0f : 0.0f);
		}

		// ---- Messages ------------------------------------------------------------------------------------

		void ShowMessage(MvpLabel first, MvpLabel second = MvpLabel::Count)
		{
			m_Message.Labels[0] = first;
			m_Message.Labels[1] = second;
			m_Message.Time = MessageTime;
			PresentMessage();
		}

		void PresentMessage()
		{
			Scene& scene = *GetEntity().GetScene();
			const bool shown = m_Message.Time > 0.0f;
			const float k = shown ? std::min(1.0f, m_Message.Time / 0.3f) : 0.0f;   // fades out at the end
			SetAlpha(scene, m_Message.Panel, 0.9f * k);
			for (int i = 0; i < 2; i++)
			{
				const bool used = shown && m_Message.Labels[i] != MvpLabel::Count;
				SetAlpha(scene, m_Message.Line[i], used ? k : 0.0f);
				if (used)
				{
					SetSprite(scene, m_Message.Line[i], (uint32_t)m_Message.Labels[i]);
				}
			}
		}

		// ---- The inventory menu ----------------------------------------------------------------------------

		void ShowMenu(bool open)
		{
			UIState::MenuOpen = open;
			if (open)
			{
				ShowPrompt(false);
				m_Selected = std::clamp(m_Selected, 0, std::max(0, (int)m_Items.size() - 1));   // the selection is kept between opens
			}
			PresentMenu();
		}

		void UpdateMenu(Scene& scene)
		{
			if (KeyPressed(KeyCode::Escape, m_Previous[2]))
			{
				ShowMenu(false);
				return;
			}
			int direction = 0;
			if (KeyPressed(KeyCode::Up, m_Previous[3]) || KeyPressed(KeyCode::W, m_Previous[4]))
			{
				direction = -1;
			}
			if (KeyPressed(KeyCode::Down, m_Previous[5]) || KeyPressed(KeyCode::S, m_Previous[6]))
			{
				direction = 1;
			}
			if (direction != 0 && m_Items.empty() == false)
			{
				m_Selected = (m_Selected + direction + (int)m_Items.size()) % (int)m_Items.size();   // wraps, like the prototype HUD
				PresentMenu();
			}
			if (KeyPressed(KeyCode::Enter, m_Previous[7]) || KeyPressed(KeyCode::Space, m_Previous[8]))
			{
				UseSelected(scene);
			}
		}

		void UseSelected(Scene& scene)
		{
			if (m_Selected < 0 || m_Selected >= (int)m_Items.size())
			{
				return;
			}
			Item& item = m_Items[m_Selected];
			if (item.Usable == false)
			{
				ShowMessage(MvpLabel::NotUsableHere);
				return;
			}
			if (item.Count <= 0)
			{
				ShowMessage(MvpLabel::NoneLeft);
				return;
			}
			item.Count -= 1;
			const int heal = item.Label == MvpLabel::HiPotion ? 60 : 25;
			for (int h = 0; h < 2; h++)
			{
				if (item.Label == MvpLabel::Ether)
				{
					BattleState::HeroMP[h] = std::min(GetHero(h).MaxMP, BattleState::HeroMP[h] + 10);
				}
				else if (BattleState::HeroHP[h] > 0)
				{
					BattleState::HeroHP[h] = std::min(GetHero(h).MaxHP, BattleState::HeroHP[h] + heal);
				}
			}
			ShowMessage(MvpLabel::UsedPotion);
			HY_INFO("MVP: used item {} ({} left): Orin {} HP {} MP, Lila {} HP {} MP", (int)item.Label, item.Count,
				BattleState::HeroHP[0], BattleState::HeroMP[0], BattleState::HeroHP[1], BattleState::HeroMP[1]);
			(void)scene;
			PresentMenu();
		}

		void PresentMenu()
		{
			Scene& scene = *GetEntity().GetScene();
			const bool open = UIState::MenuOpen;
			SetAlpha(scene, m_Menu.Panel, open ? 0.92f : 0.0f);
			SetAlpha(scene, m_Menu.Title, open ? 1.0f : 0.0f);
			SetAlpha(scene, m_Menu.Footer[0], open ? 1.0f : 0.0f);
			SetAlpha(scene, m_Menu.Footer[1], open ? 1.0f : 0.0f);
			// scroll so the selected row is visible (the prototype's ScrollTo)
			if (m_Selected < m_Scroll)
			{
				m_Scroll = m_Selected;
			}
			if (m_Selected >= m_Scroll + VisibleRows)
			{
				m_Scroll = m_Selected - VisibleRows + 1;
			}
			m_Scroll = std::clamp(m_Scroll, 0, std::max(0, (int)m_Items.size() - VisibleRows));
			for (int i = 0; i < VisibleRows; i++)
			{
				const int index = m_Scroll + i;
				const bool used = open && index < (int)m_Items.size();
				SetAlpha(scene, m_Menu.Row[i], used ? 1.0f : 0.0f);
				SetAlpha(scene, m_Menu.Count[i], used ? 1.0f : 0.0f);
				if (used)
				{
					SetSprite(scene, m_Menu.Row[i], (uint32_t)m_Items[index].Label);
					SetSprite(scene, m_Menu.Count[i], 12 + (uint32_t)std::clamp(m_Items[index].Count, 0, 9));   // BattleIcons' digit row
				}
			}
			const int row = m_Selected - m_Scroll;
			SetAlpha(scene, m_Menu.Highlight, open ? 0.55f : 0.0f);
			SetAlpha(scene, m_Menu.Cursor, open ? 1.0f : 0.0f);
			SetOffset(m_Menu.Highlight, { m_HighlightOffset.x, m_RowOffset.y + m_RowStep * (float)row });
			SetOffset(m_Menu.Cursor, { m_CursorOffset.x, m_RowOffset.y + m_RowStep * (float)row });
		}

		// ---- The UI follows the camera -----------------------------------------------------------------

		struct Offset
		{
			entt::entity Entity = entt::null;
			glm::vec2 Value{ 0.0f };
		};

		glm::vec2 OffsetOf(entt::entity entity) const
		{
			for (const Offset& offset : m_Offsets)
			{
				if (offset.Entity == entity)
				{
					return offset.Value;
				}
			}
			return glm::vec2(0.0f);
		}

		void SetOffset(entt::entity entity, const glm::vec2& value)
		{
			for (Offset& offset : m_Offsets)
			{
				if (offset.Entity == entity)
				{
					offset.Value = value;
				}
			}
		}

		void FollowCamera(Scene& scene)
		{
			const glm::vec2 camera = Position(scene, m_Camera);
			for (const Offset& offset : m_Offsets)
			{
				SetPosition(scene, offset.Entity, camera + offset.Value);
			}
		}

		// ---- Helpers -------------------------------------------------------------------------------------

		static bool KeyPressed(KeyCode key, bool& previous)
		{
			const bool down = Input::IsKeyPressed(key);
			const bool pressed = down && previous == false;
			previous = down;
			return pressed;
		}

		static entt::entity Find(Scene& scene, const std::string& tag)
		{
			auto view = scene.m_Registry.view<TagComponent>();
			for (auto entity : view)
			{
				if (view.get<TagComponent>(entity).Tag == tag)
				{
					return entity;
				}
			}
			HY_WARN("MVP: the scene has no entity tagged \"{}\"", tag);
			return entt::null;
		}

		static bool Alive(Scene& scene, entt::entity entity)
		{
			return entity != entt::null && scene.m_Registry.valid(entity);
		}

		static glm::vec2 Position(Scene& scene, entt::entity entity)
		{
			if (Alive(scene, entity) && scene.m_Registry.has<TransformComponent>(entity))
			{
				const glm::vec3& position = scene.m_Registry.get<TransformComponent>(entity).Translation;
				return { position.x, position.y };
			}
			return glm::vec2(0.0f);
		}

		static void SetPosition(Scene& scene, entt::entity entity, const glm::vec2& position)
		{
			if (Alive(scene, entity) && scene.m_Registry.has<TransformComponent>(entity))
			{
				TransformComponent& transform = scene.m_Registry.get<TransformComponent>(entity);
				transform.Translation.x = position.x;
				transform.Translation.y = position.y;
			}
		}

		static void SetAlpha(Scene& scene, entt::entity entity, float alpha)
		{
			if (Alive(scene, entity) && scene.m_Registry.has<SpriteRendererComponent>(entity))
			{
				scene.m_Registry.get<SpriteRendererComponent>(entity).Color.a = alpha;
			}
		}

		static void SetSprite(Scene& scene, entt::entity entity, uint32_t index)
		{
			if (Alive(scene, entity) && scene.m_Registry.has<SpriteRendererComponent>(entity))
			{
				scene.m_Registry.get<SpriteRendererComponent>(entity).SpriteIndex = index;
			}
		}

	private:
		struct PromptUI
		{
			entt::entity Panel = entt::null;
			entt::entity Label = entt::null;
		};

		struct MessageUI
		{
			entt::entity Panel = entt::null;
			entt::entity Line[2] = { entt::null, entt::null };
			MvpLabel Labels[2] = { MvpLabel::Count, MvpLabel::Count };
			float Time = 0.0f;
		};

		struct MenuUI
		{
			entt::entity Panel = entt::null;
			entt::entity Title = entt::null;
			entt::entity Footer[2] = { entt::null, entt::null };
			entt::entity Highlight = entt::null;
			entt::entity Cursor = entt::null;
			entt::entity Row[VisibleRows] = {};
			entt::entity Count[VisibleRows] = {};
		};

		entt::entity m_Camera = entt::null;
		entt::entity m_Player = entt::null;
		entt::entity m_Fade = entt::null;
		PromptUI m_Prompt;
		MessageUI m_Message;
		MenuUI m_Menu;
		std::vector<Offset> m_Offsets;
		glm::vec2 m_HighlightOffset{ 0.0f };
		glm::vec2 m_CursorOffset{ 0.0f };
		glm::vec2 m_RowOffset{ 0.0f };
		float m_RowStep = -0.55f;
		std::vector<Interactable> m_Interactables;
		std::vector<Item> m_Items;
		int m_Selected = 0;
		int m_Scroll = 0;
		int m_NpcLine = 0;
		float m_Time = 0.0f;
		float m_Leaving = 0.0f;
		float m_Resting = 0.0f;
		bool m_Previous[9] = {};
	};
}
