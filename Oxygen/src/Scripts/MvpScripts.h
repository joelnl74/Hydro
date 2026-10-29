#pragma once
#include "Hydro.h"
#include "VillageScripts.h"
#include "BattleScripts.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

// The MVP scene's mechanics (docs/DioramaRPG.md, the GDD's first-scene test), used by the field scenes (the
// Shrine Glade, the Farm Village): interact prompts (a sign, a chest, the inn's door, a save point, the map's
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

	HY_SCRIPT()
	class MvpDirectorScript : public ScriptableEntity
	{
	public:
		// Interaction: what counts as in reach of the player.
		HY_PROPERTY(Range(0, 20), Tooltip("Metres: farther items get no prompt"))
		float InteractRange = 1.7f;
		HY_PROPERTY(Range(0, 10), Tooltip("Metres between the player's ground and the item's (another terrace is out of reach)"))
		float MaxReachHeight = 1.0f;
		HY_PROPERTY(Range(0, 180), Tooltip("Degrees either side of where the player faces"))
		float FacingDegrees = 80.0f;
		HY_PROPERTY(Range(0, 10), Tooltip("Metres: this close, an item is in reach whichever way the player faces"))
		float CloseRange = 0.6f;
		HY_PROPERTY(Range(0, 5), Tooltip("Metres another item must be closer by to take the prompt over (stops flicker)"))
		float SwitchMargin = 0.3f;
		HY_PROPERTY(Range(0, 5), Tooltip("Line of sight starts this high above the player's ground (the chest)"))
		float EyeHeight = 1.0f;
		HY_PROPERTY(Range(0, 5), Tooltip("...and ends this high on the item"))
		float ItemHeight = 0.6f;

		// Timing, in seconds.
		HY_PROPERTY(Range(0.1, 30), Tooltip("How long a message stays up"))
		float MessageTime = 3.0f;
		HY_PROPERTY(Range(0.05, 5), Tooltip("Fade out when leaving through an exit"))
		float LeaveTime = 0.6f;
		HY_PROPERTY(Range(0.05, 5), Tooltip("Fade in after arriving"))
		float ArriveTime = 0.6f;
		HY_PROPERTY(Range(0.05, 10), Tooltip("Fade out and in when resting at a save point"))
		float RestTime = 1.2f;

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

			// The UI entities are authored around the origin: their positions (in the world) are offsets from the camera.
			auto remember = [&](entt::entity entity)
			{
				if (Alive(scene, entity))
				{
					m_Offsets.push_back({ entity, Position(scene, entity) });
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
				else if (tag.rfind("Exit", 0) == 0) { item.Type = Kind::Exit; item.Target = tag.size() > 5 ? tag.substr(5) : std::string(); }
				else if (tag.rfind("Npc", 0) == 0) { item.Type = Kind::Npc; }
				else { continue; }
				if (scene.m_Registry.has<SpriteRendererComponent>(entity))
				{
					// a sprite's entity sits at its middle; the player talks to its feet
					item.FeetDrop = 0.5f * view.get<TransformComponent>(entity).Scale.y;
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
			// Arriving (the scene was just loaded, or the player came back to it): fade in from black.
			m_Arriving = ArriveTime;
			SetAlpha(scene, m_Fade, 1.0f);
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
					Leave(m_LeavingTo);
					// Kept as it is while the next scene runs: when the player comes back here it fades in.
					m_Arriving = ArriveTime;
				}
				return;
			}
			if (m_Arriving > 0.0f)
			{
				m_Arriving = std::max(0.0f, m_Arriving - deltaTime);
				SetAlpha(scene, m_Fade, m_Arriving / ArriveTime);
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
			float FeetDrop = 0.0f; // from its position (in the world, read when needed: it may have a parent) to its feet
			bool Used = false;   // chests open once
			std::string Target;  // exits: the scene they lead to (the tag after "Exit ")
		};

		struct Item
		{
			MvpLabel Label = MvpLabel::Potion;
			int Count = 0;
			bool Usable = false;
		};

		static constexpr int VisibleRows = 6;

		// ---- Interaction --------------------------------------------------------------------------------

		void UpdateInteraction(Scene& scene)
		{
			const glm::vec2 player = Position(scene, m_Player);
			// The ground the player stands on (the character's feet in the field scenes).
			const float playerHeight = Height(scene, m_Player);

			// Where the player faces: the way it last walked.
			const glm::vec2 walked = player - m_LastPlayer;
			if (glm::dot(walked, walked) > 1.0e-6f)
			{
				m_Facing = glm::normalize(walked);
			}
			m_LastPlayer = player;

			// The triggers the player stands in (an exit's area), from the 3D world's colliders.
			std::vector<Entity> inside;
			Physics3D::OverlapSphere(scene, glm::vec3(player, playerHeight + 0.5f), 0.3f, Physics3D::LayerBit(PhysicsLayer::Trigger), inside);

			Interactable* nearest = nullptr;
			float best = InteractRange;
			float currentDistance = -1.0f;
			for (Interactable& item : m_Interactables)
			{
				float distance = 0.0f;
				if (InReach(scene, item, player, playerHeight, inside, distance) == false)
				{
					continue;
				}
				if (item.Entity == m_Current)
				{
					currentDistance = distance;
				}
				if (distance < best)
				{
					best = distance;
					nearest = &item;
				}
			}
			// Stable: the prompt stays on what it shows until something else is clearly closer.
			if (nearest != nullptr && nearest->Entity != m_Current && currentDistance >= 0.0f && best > currentDistance - SwitchMargin)
			{
				for (Interactable& item : m_Interactables)
				{
					if (item.Entity == m_Current)
					{
						nearest = &item;
					}
				}
			}
			m_Current = nearest != nullptr ? nearest->Entity : entt::null;

			ShowPrompt(nearest != nullptr);
			if (nearest == nullptr)
			{
				return;
			}
			SetSprite(scene, m_Prompt.Label, (uint32_t)PromptFor(nearest->Type));
			// Above the player's head, in either camera view (DioramaCamera.h).
			const glm::vec3 overHead = DioramaView::Above(glm::vec3(player, playerHeight), 2.35f);
			SetPosition(scene, m_Prompt.Panel, overHead);
			SetPosition(scene, m_Prompt.Label, overHead);

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
					if (nearest->Target.empty())
					{
						ShowMessage(MvpLabel::RoadGoesOn);
						break;
					}
					ShowMessage(MvpLabel::RoadGoesOn, nearest->Target == "FarmVillage" ? MvpLabel::ToTheVillage : MvpLabel::Count);
					m_LeavingTo = nearest->Target;
					m_Leaving = LeaveTime;
					HY_INFO("MVP: leaving for {}", m_LeavingTo);
					break;
				case Kind::Npc:
					ShowMessage((MvpLabel)((uint32_t)MvpLabel::NpcLine1 + m_NpcLine));
					m_NpcLine = (m_NpcLine + 1) % 3;
					break;
			}
		}

		// Whether the player can use `item` now, and how far it is (the prompt goes to the nearest). An exit with a
		// trigger collider is in reach while the player stands in it. Anything else must be within InteractRange, at
		// about the player's height (not on the terrace above), in front of the player (unless very close) and in
		// sight: a ray from the player's chest to the item hits nothing solid before it (Physics3D).
		bool InReach(Scene& scene, const Interactable& item, const glm::vec2& player, float playerHeight, const std::vector<Entity>& inside, float& distance) const
		{
			if (Alive(scene, item.Entity) == false)
			{
				return false;
			}

			const Entity entity{ item.Entity, &scene };
			const bool hasTrigger = (entity.HasComponent<BoxColliderComponent>() && entity.GetComponent<BoxColliderComponent>().IsTrigger)
				|| (entity.HasComponent<SphereColliderComponent>() && entity.GetComponent<SphereColliderComponent>().IsTrigger);
			if (item.Type == Kind::Exit && hasTrigger && Physics3D::HasBody(entity))
			{
				distance = 0.0f;
				for (const Entity& trigger : inside)
				{
					if ((entt::entity)trigger == item.Entity)
					{
						return true;
					}
				}
				return false;
			}

			const float itemHeight = Height(scene, item.Entity);
			if (std::abs(itemHeight - playerHeight) > MaxReachHeight)
			{
				return false;
			}
			const glm::vec2 position = Position(scene, item.Entity) - glm::vec2(0.0f, item.FeetDrop);
			const glm::vec2 offset = position - player;
			distance = glm::length(offset);
			if (distance >= InteractRange)
			{
				return false;
			}
			if (distance > CloseRange && glm::dot(offset / distance, m_Facing) < std::cos(glm::radians(FacingDegrees)))
			{
				return false;
			}

			const glm::vec3 eye{ player, playerHeight + EyeHeight };
			const glm::vec3 target{ position, itemHeight + ItemHeight };
			RaycastHit3D hit;
			if (Physics3D::Raycast(scene, eye, target, Physics3D::SolidLayers, hit))
			{
				const bool hitItem = (entt::entity)hit.HitEntity == item.Entity;
				const bool pastItem = hit.Distance >= glm::length(target - eye) - 0.15f;
				if (hitItem == false && pastItem == false)
				{
					return false;
				}
			}
			return true;
		}

		// An exit's tag names the scene it leads to ("Exit FarmVillage"). Field scenes connect in a line for now
		// (the Shrine Glade <-> the Farm Village), so a scene that was pushed by another goes back to it (Pop: the
		// one below is exactly as the player left it, at that exit), and the first scene pushes the next. A world
		// map or scene graph replaces this when there are more than two field scenes.
		static void Leave(const std::string& target)
		{
			if (SceneTransition::GetDepth() > 0)
			{
				SceneTransition::Pop();
			}
			else
			{
				SceneTransition::Push(target);
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
			for (int h = 0; h < HeroCount; h++)
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
			HY_INFO("MVP: used item {} ({} left): {} {} HP {} MP", (int)item.Label, item.Count,
				GetHero(0).Name, BattleState::HeroHP[0], BattleState::HeroMP[0]);
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

		// Every UI sprite at its offset from the centre of the screen, on the camera's view plane through the
		// point it looks at (DioramaView::OnScreen; top-down: the camera's x, y plus the offset, at z 0.5).
		// The UI sprites are Billboards, so they face the camera when it is tilted.
		void FollowCamera([[maybe_unused]] Scene& scene)
		{
			for (const Offset& offset : m_Offsets)
			{
				SetPosition(scene, offset.Entity, DioramaView::OnScreen(offset.Value));
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

		// Positions are in the world (Scene::GetWorldPosition / SetWorldPosition), so these entities may sit under
		// any parent.
		static glm::vec2 Position(Scene& scene, entt::entity entity)
		{
			if (Alive(scene, entity) && scene.m_Registry.has<TransformComponent>(entity))
			{
				return glm::vec2(scene.GetWorldPosition(Entity{ entity, &scene }));
			}
			return glm::vec2(0.0f);
		}

		static float Height(Scene& scene, entt::entity entity)
		{
			if (Alive(scene, entity) && scene.m_Registry.has<TransformComponent>(entity))
			{
				return scene.GetWorldPosition(Entity{ entity, &scene }).z;
			}
			return 0.0f;
		}

		static void SetPosition(Scene& scene, entt::entity entity, const glm::vec2& position)
		{
			if (Alive(scene, entity) && scene.m_Registry.has<TransformComponent>(entity))
			{
				const Entity handle{ entity, &scene };
				const float height = scene.GetWorldPosition(handle).z;
				scene.SetWorldPosition(handle, glm::vec3(position, height));
			}
		}

		static void SetPosition(Scene& scene, entt::entity entity, const glm::vec3& position)
		{
			if (Alive(scene, entity) && scene.m_Registry.has<TransformComponent>(entity))
			{
				scene.SetWorldPosition(Entity{ entity, &scene }, position);
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
		entt::entity m_Current = entt::null; // what the prompt shows
		glm::vec2 m_LastPlayer{ 0.0f };
		glm::vec2 m_Facing{ 0.0f, 1.0f };
		std::vector<Item> m_Items;
		int m_Selected = 0;
		int m_Scroll = 0;
		int m_NpcLine = 0;
		float m_Time = 0.0f;
		float m_Leaving = 0.0f;
		float m_Arriving = 0.0f;
		float m_Resting = 0.0f;
		std::string m_LeavingTo;
		bool m_Previous[9] = {};
	};
}
