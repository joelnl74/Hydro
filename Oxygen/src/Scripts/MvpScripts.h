#pragma once
#include "Hydro.h"
#include "VillageScripts.h"
#include "BattleScripts.h"
#include "DialogueScripts.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

// The MVP scene's mechanics (docs/DioramaRPG.md, the GDD's first-scene test), used by the field scenes (the
// Shrine Glade, the Farm Village): interact prompts (a sign, a chest, the inn's door, a save point, the map's
// exits, an NPC) and a sample inventory menu that gates the player's input while it is open. Prototype
// code, like BattleScripts.h: one script holds it all. The UI is runtime UI under the MVP Director on the 1920 x 1080
// canvas (scripts/ui/build_game_ui.py builds it): the inventory (a scrolling list, the item's details, the party, a
// help bar), the message banner, the fade, and the "E  Talk" prompt as a World canvas over the player's head; NPCs
// talk through the dialogue box (DialogueScripts.h).
namespace Hydro
{
	// The MVP's messages and prompts (once rows of DioramaRPG/UI/MvpText.png; the runtime UI writes their words,
	// GetMvpText). The item labels are no longer used: items have names of their own (MvpDirectorScript::Item).
	enum class MvpLabel : uint32_t
	{
		Inventory, EnterUse, EscClose,
		Potion, HiPotion, Ether, IronSword, HerbBundle, PhoenixFeather, CrystalShard, LeatherBoots, Antidote, AncientKey, CopperOre, OldMap,
		PromptRead, PromptOpen, PromptEnter, PromptSave, PromptLeave, PromptTalk,
		SignLine1, SignLine2, FoundPotion, ChestEmpty, Rested, RestedLine2, NoSaveSlot, RoadGoesOn, ToTheVillage,
		UsedPotion, NotUsableHere, NoneLeft, NpcLine1, NpcLine2, NpcLine3,
		Count
	};

	// The words of a label, for the runtime UI.
	inline const char* GetMvpText(MvpLabel label)
	{
		switch (label)
		{
		case MvpLabel::Inventory: return "Inventory";
		case MvpLabel::EnterUse: return "Enter: use";
		case MvpLabel::EscClose: return "Esc: close";
		case MvpLabel::Potion: return "Potion";
		case MvpLabel::HiPotion: return "Hi-Potion";
		case MvpLabel::Ether: return "Ether";
		case MvpLabel::IronSword: return "Iron Sword";
		case MvpLabel::HerbBundle: return "Herb Bundle";
		case MvpLabel::PhoenixFeather: return "Phoenix Feather";
		case MvpLabel::CrystalShard: return "Crystal Shard";
		case MvpLabel::LeatherBoots: return "Leather Boots";
		case MvpLabel::Antidote: return "Antidote";
		case MvpLabel::AncientKey: return "Ancient Key";
		case MvpLabel::CopperOre: return "Copper Ore";
		case MvpLabel::OldMap: return "Old Map";
		case MvpLabel::PromptRead: return "E: Read";
		case MvpLabel::PromptOpen: return "E: Open";
		case MvpLabel::PromptEnter: return "E: Enter";
		case MvpLabel::PromptSave: return "E: Save";
		case MvpLabel::PromptLeave: return "E: Leave";
		case MvpLabel::PromptTalk: return "E: Talk";
		case MvpLabel::SignLine1: return "Shrine Glade Inn";
		case MvpLabel::SignLine2: return "Village: south";
		case MvpLabel::FoundPotion: return "Found a Potion!";
		case MvpLabel::ChestEmpty: return "The chest is empty.";
		case MvpLabel::Rested: return "You feel rested.";
		case MvpLabel::RestedLine2: return "HP and MP restored.";
		case MvpLabel::NoSaveSlot: return "No save slot yet.";
		case MvpLabel::RoadGoesOn: return "The road goes on...";
		case MvpLabel::ToTheVillage: return "To the village.";
		case MvpLabel::UsedPotion: return "Used an item.";
		case MvpLabel::NotUsableHere: return "Not usable here.";
		case MvpLabel::NoneLeft: return "None left.";
		case MvpLabel::NpcLine1: return "Lovely evening.";
		case MvpLabel::NpcLine2: return "Beware the wisps.";
		case MvpLabel::NpcLine3: return "The inn is warm.";
		default: return "";
		}
	}

	// What a field NPC says, by the name after "Npc " in its tag (lines in code until there is a dialogue asset).
	inline std::vector<std::string> GetNpcLines(const std::string& name)
	{
		if (name == "Elder")
		{
			return {
				"A lovely evening at the shrine, isn't it? The lanterns are lit early these days.",
				"Beware the wisps that drift through the meadow after dark. They follow travellers who walk alone.",
				"If you're weary, the inn down in the village is warm. Rest there before you go on."
			};
		}
		if (name == "Farmer")
		{
			return {
				"The wheat came in heavy this year. Good soil, this side of the stream.",
				"Slimes keep getting into the cabbages, though. Little thieves."
			};
		}
		if (name == "Gatherer")
		{
			return {
				"I pick herbs along the shrine path every morning. The best ones grow by the old stones.",
				"If you find a Herb Bundle, keep it. Someone in the village will want it."
			};
		}
		if (name == "Barmaid")
		{
			return {
				"Welcome! The inn's door is just there. A night's rest puts anyone right.",
				"Travellers say the road east goes all the way to the sea. I wouldn't know."
			};
		}
		if (name == "Blacksmith")
		{
			return {
				"Mind the sparks. An iron sword takes a week if you want it done properly.",
				"Bring me copper ore and I'll see what I can do. Not today, though."
			};
		}
		return { "Lovely evening.", "Beware the wisps.", "The inn is warm." };
	}

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
			m_Prompt.Canvas = Find(scene, "Prompt Canvas");
			m_Prompt.Text = Find(scene, "Prompt Text");
			// The runtime UI (canvases under this director, so their buttons' events reach OnUIEvent).
			m_Fade = Find(scene, "Fade");
			m_Message.Panel = Find(scene, "Field Message Panel");
			m_Message.Text = Find(scene, "Field Message Text");
			m_Menu.Canvas = Find(scene, "Inventory Canvas");
			for (int i = 0; i < MaxRows; i++)
			{
				m_Menu.Row[i] = Find(scene, "Item " + std::to_string(i));
				m_Menu.Name[i] = Find(scene, "Item " + std::to_string(i) + " Name");
				m_Menu.Count[i] = Find(scene, "Item " + std::to_string(i) + " Count");
			}
			m_Menu.DetailName = Find(scene, "Item Detail Name");
			m_Menu.DetailText = Find(scene, "Item Detail Text");
			m_Menu.Help = Find(scene, "Help Text");
			m_Menu.StatusName = Find(scene, "Menu Status Miko Name");
			m_Menu.StatusHP = Find(scene, "Menu Status Miko HP");
			m_Menu.StatusMP = Find(scene, "Menu Status Miko MP");
			m_Menu.StatusHPFill = Find(scene, "Menu Status Miko HP Fill");
			m_Menu.StatusMPFill = Find(scene, "Menu Status Miko MP Fill");
			m_DialogueCanvas = Find(scene, "Dialogue Canvas");

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
				else if (tag.rfind("Npc", 0) == 0) { item.Type = Kind::Npc; item.Target = tag.size() > 4 ? tag.substr(4) : std::string(); }
				else { continue; }
				if (scene.m_Registry.has<SpriteRendererComponent>(entity))
				{
					// a sprite's entity sits at its middle; the player talks to its feet
					item.FeetDrop = 0.5f * view.get<TransformComponent>(entity).Scale.y;
				}
				m_Interactables.push_back(item);
			}

			// The sample inventory: twenty items, more than the list shows, so it scrolls (the first is the Potion the
			// chest adds to). HP and MP restore every hero; 0 = not usable here.
			m_Items = {
				{ "Potion", "A bitter brew of the shrine's herbs. Restores 25 HP.", 3, 25, 0 },
				{ "Hi-Potion", "A stronger draught, sealed with wax. Restores 60 HP.", 1, 60, 0 },
				{ "Ether", "Water drawn at dawn from the sacred spring. Restores 10 MP.", 2, 0, 10 },
				{ "Herb Tea", "Warm tea of mountain herbs. Restores 15 HP.", 2, 15, 0 },
				{ "Iron Sword", "A plain blade from the village smithy. Miko fights bare-handed.", 1, 0, 0 },
				{ "Herb Bundle", "Fresh herbs tied with twine. Someone in the village may want them.", 4, 0, 0 },
				{ "Phoenix Feather", "A feather that is warm to the touch. What it is for, nobody knows yet.", 1, 0, 0 },
				{ "Crystal Shard", "A shard that hums faintly near the shrine.", 2, 0, 0 },
				{ "Leather Boots", "Sturdy boots for the mountain road.", 1, 0, 0 },
				{ "Antidote", "Cures poison. Nothing here is poisoned.", 2, 0, 0 },
				{ "Ancient Key", "An old bronze key with a lantern crest.", 1, 0, 0 },
				{ "Copper Ore", "A lump of ore for the blacksmith.", 5, 0, 0 },
				{ "Old Map", "A faded map. The road east is marked in red.", 1, 0, 0 },
				{ "Shrine Charm", "A paper charm from the shrine. It keeps the wisps at a distance, they say.", 1, 0, 0 },
				{ "Lantern Oil", "Oil for the stone lanterns along the path.", 3, 0, 0 },
				{ "Rope", "A coil of hemp rope.", 1, 0, 0 },
				{ "Smoked Fish", "Smoked river trout, wrapped in leaves.", 2, 0, 0 },
				{ "Spring Water", "Cold water from the shrine spring.", 3, 0, 0 },
				{ "Pine Resin", "Sticky resin from the glade's pines.", 2, 0, 0 },
				{ "Fox Mask", "A festival mask, painted white and red.", 1, 0, 0 }
			};

			ShowMenu(false);
			ShowPrompt(false);
			m_Message.Time = 0.0f;
			PresentMessage();
			// Arriving (the scene was just loaded, or the player came back to it): fade in from black.
			m_Arriving = ArriveTime;
			SetFade(1.0f);
			HY_INFO("MVP: WASD walk, E interact / talk (Enter next line, Esc close), Tab inventory (Up/Down select, Enter or click use, Esc close), 1/2/3 day-dusk-night");
		}

		void OnUpdate(float deltaTime) override
		{
			Scene& scene = *GetEntity().GetScene();
			m_Time += deltaTime;

			// a scene change in progress: fade, then go
			if (m_Leaving > 0.0f)
			{
				m_Leaving = std::max(0.0f, m_Leaving - deltaTime);
				SetFade(1.0f - m_Leaving / LeaveTime);
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
				SetFade(m_Arriving / ArriveTime);
			}
			// a rest at the inn: a dip to black and back
			if (m_Resting > 0.0f)
			{
				m_Resting = std::max(0.0f, m_Resting - deltaTime);
				const float k = m_Resting / RestTime;
				SetFade(k > 0.5f ? (1.0f - k) * 2.0f : k * 2.0f);
			}

			// A runtime UI canvas that captures input (UI::CapturesInput: the inventory, the dialogue, any menu) owns
			// the keys: no interaction. Tab is the inventory's own key, so it also closes it.
			const bool menuOpen = UI::IsVisible(EntityOf(m_Menu.Canvas));
			if (KeyPressed(KeyCode::Tab, m_Previous[0]) && (menuOpen || UI::CapturesInput(scene) == false))
			{
				ShowMenu(menuOpen == false);
			}
			if (UI::CapturesInput(scene))
			{
				ShowPrompt(false);
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

		// The inventory's rows ("Item 0" to "Item 19") bubble their events up to the director: the focus is the
		// selection (kept between opens), Submit uses the item, Cancel closes the menu.
		bool OnUIEvent(const UIEvent& event) override
		{
			for (int i = 0; i < MaxRows; i++)
			{
				if ((entt::entity)event.Target != m_Menu.Row[i])
				{
					continue;
				}
				switch (event.Type)
				{
				case UIEventType::FocusEnter:
					m_Selected = i;
					PresentDetails();
					return true;
				case UIEventType::Submit:
					m_Selected = i;
					UseSelected();
					return true;
				case UIEventType::Cancel:
					ShowMenu(false);
					return true;
				default:
					return true;
				}
			}
			if ((entt::entity)event.Target == m_Menu.Canvas && event.Type == UIEventType::Cancel)
			{
				ShowMenu(false);
				return true;
			}
			return false;
		}

	private:
		enum class Kind : uint8_t { Sign, Chest, Door, Save, Exit, Npc };

		struct Interactable
		{
			entt::entity Entity = entt::null;
			Kind Type = Kind::Sign;
			float FeetDrop = 0.0f; // from its position (in the world, read when needed: it may have a parent) to its feet
			bool Used = false;   // chests open once
			std::string Target;  // exits: the scene they lead to (the tag after "Exit "); NPCs: their name (after "Npc ")
		};

		struct Item
		{
			std::string Name;
			std::string Description;
			int Count = 0;
			int RestoreHP = 0;  // to every hero; with RestoreMP 0 too, the item can't be used here
			int RestoreMP = 0;
			bool IsUsable() const { return RestoreHP > 0 || RestoreMP > 0; }
		};

		static constexpr int MaxRows = 20; // the inventory's rows in the scene ("Item 0" to "Item 19"); more items aren't shown

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
			UI::SetText(EntityOf(m_Prompt.Text), PromptFor(nearest->Type));
			// Above the player's head, in either camera view (DioramaCamera.h): the World canvas sits on that point.
			const glm::vec3 overHead = DioramaView::Above(glm::vec3(player, playerHeight), 2.35f);
			SetPosition(scene, m_Prompt.Canvas, overHead);

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
					if (DialogueScript* dialogue = FindDialogue(scene))
					{
						dialogue->Open(nearest->Target, GetNpcLines(nearest->Target));
						ShowPrompt(false);
					}
					else
					{
						// A scene without the dialogue box: one line in the message box.
						ShowMessage((MvpLabel)((uint32_t)MvpLabel::NpcLine1 + m_NpcLine));
						m_NpcLine = (m_NpcLine + 1) % 3;
					}
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

		static const char* PromptFor(Kind kind)
		{
			switch (kind)
			{
				case Kind::Chest: return "E  Open";
				case Kind::Door: return "E  Enter";
				case Kind::Save: return "E  Save";
				case Kind::Exit: return "E  Leave";
				case Kind::Npc: return "E  Talk";
				default: return "E  Read";
			}
		}

		void ShowPrompt(bool shown)
		{
			Entity prompt = EntityOf(m_Prompt.Canvas);
			if (UI::IsVisible(prompt) != shown)
			{
				UI::SetVisible(prompt, shown);
			}
		}

		// ---- Messages ------------------------------------------------------------------------------------

		void ShowMessage(MvpLabel first, MvpLabel second = MvpLabel::Count)
		{
			m_Message.Labels[0] = first;
			m_Message.Labels[1] = second;
			m_Message.Time = MessageTime;
			PresentMessage();
		}

		// The message box (runtime UI): one or two lines, fading out at the end.
		void PresentMessage()
		{
			const bool shown = m_Message.Time > 0.0f;
			const float k = shown ? std::min(1.0f, m_Message.Time / 0.3f) : 0.0f;
			Entity panel = EntityOf(m_Message.Panel);
			if (UI::IsVisible(panel) != shown)
			{
				UI::SetVisible(panel, shown);
			}
			if (shown == false)
			{
				return;
			}
			std::string text = GetMvpText(m_Message.Labels[0]);
			if (m_Message.Labels[1] != MvpLabel::Count)
			{
				text += std::string("\n") + GetMvpText(m_Message.Labels[1]);
			}
			Entity textEntity = EntityOf(m_Message.Text);
			UI::SetText(textEntity, text);
			SetUIAlpha(panel, 0.92f * k);
			SetUIAlpha(textEntity, k);
		}

		// ---- The inventory menu ----------------------------------------------------------------------------

		void ShowMenu(bool open)
		{
			Entity canvas = EntityOf(m_Menu.Canvas);
			if (canvas.IsValid() == false)
			{
				return;
			}
			UI::SetVisible(canvas, open);
			if (open)
			{
				ShowPrompt(false);
				PresentMenu();
				// The selection is kept between opens.
				m_Selected = std::clamp(m_Selected, 0, std::max(0, std::min((int)m_Items.size(), MaxRows) - 1));
				UI::Focus(EntityOf(m_Menu.Row[m_Selected]));
			}
		}

		void UseSelected()
		{
			if (m_Selected < 0 || m_Selected >= (int)m_Items.size())
			{
				return;
			}
			Item& item = m_Items[m_Selected];
			if (item.IsUsable() == false)
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
			for (int h = 0; h < HeroCount; h++)
			{
				BattleState::HeroMP[h] = std::min(GetHero(h).MaxMP, BattleState::HeroMP[h] + item.RestoreMP);
				if (BattleState::HeroHP[h] > 0)
				{
					BattleState::HeroHP[h] = std::min(GetHero(h).MaxHP, BattleState::HeroHP[h] + item.RestoreHP);
				}
			}
			ShowMessage(MvpLabel::UsedPotion);
			HY_INFO("MVP: used {} ({} left): {} {} HP {} MP", item.Name, item.Count, GetHero(0).Name, BattleState::HeroHP[0], BattleState::HeroMP[0]);
			PresentMenu();
		}

		// The inventory's rows: one per item (rows past the list are hidden, so the panel fits it), name and count;
		// items that can't be used here are greyed, items used up dimmer still. Every row can take the focus.
		void PresentMenu()
		{
			for (int i = 0; i < MaxRows; i++)
			{
				const bool used = i < (int)m_Items.size();
				Entity row = EntityOf(m_Menu.Row[i]);
				if (UI::IsVisible(row) != used)
				{
					UI::SetVisible(row, used);
				}
				if (used == false)
				{
					continue;
				}
				const Item& item = m_Items[i];
				Entity name = EntityOf(m_Menu.Name[i]);
				UI::SetText(name, item.Name);
				UI::SetText(EntityOf(m_Menu.Count[i]), "x" + std::to_string(item.Count));
				if (name.IsValid() && name.HasComponent<UITextComponent>())
				{
					const glm::vec3 color = item.IsUsable() == false ? glm::vec3(0.74f, 0.71f, 0.64f)
						: (item.Count <= 0 ? glm::vec3(0.5f) : glm::vec3(0.95f, 0.91f, 0.83f));
					name.GetComponent<UITextComponent>().Color = glm::vec4(color, 1.0f);
				}
			}
			PresentDetails();

			// The party's status beside the list (the first hero; the MVP has one).
			const HeroDefinition& hero = GetHero(0);
			UI::SetText(EntityOf(m_Menu.StatusName), hero.Name);
			UI::SetText(EntityOf(m_Menu.StatusHP), std::to_string(BattleState::HeroHP[0]) + " / " + std::to_string(hero.MaxHP));
			UI::SetText(EntityOf(m_Menu.StatusMP), std::to_string(BattleState::HeroMP[0]) + " / " + std::to_string(hero.MaxMP));
			SetBarFill(EntityOf(m_Menu.StatusHPFill), (float)BattleState::HeroHP[0] / (float)hero.MaxHP);
			SetBarFill(EntityOf(m_Menu.StatusMPFill), (float)BattleState::HeroMP[0] / (float)hero.MaxMP);
		}

		// The selected item's name and description, and the help line.
		void PresentDetails()
		{
			if (m_Selected < 0 || m_Selected >= (int)m_Items.size())
			{
				return;
			}
			const Item& item = m_Items[m_Selected];
			UI::SetText(EntityOf(m_Menu.DetailName), item.Name);
			UI::SetText(EntityOf(m_Menu.DetailText), item.Description);
			UI::SetText(EntityOf(m_Menu.Help), item.IsUsable() ? (item.Count > 0 ? "Use it now." : "None left.") : "Can't be used here.");
		}

		// A UI bar's fill, anchored to the bar's left edge, as wide as the fraction.
		static void SetBarFill(Entity fill, float fraction)
		{
			if (fill.IsValid() && fill.HasComponent<UIRectComponent>())
			{
				UIRectComponent& rect = fill.GetComponent<UIRectComponent>();
				rect.AnchorMin = { 0.0f, 0.0f };
				rect.AnchorMax = { std::clamp(fraction, 0.0f, 1.0f), 1.0f };
				rect.SizeDelta = { 0.0f, 0.0f };
			}
		}

		// The dialogue box's script (on "Dialogue Canvas"), or nullptr in a scene without one.
		DialogueScript* FindDialogue(Scene& scene)
		{
			if (Alive(scene, m_DialogueCanvas) == false || scene.m_Registry.has<NativeScriptComponent>(m_DialogueCanvas) == false)
			{
				return nullptr;
			}
			for (NativeScriptComponent::Script& script : scene.m_Registry.get<NativeScriptComponent>(m_DialogueCanvas).Scripts)
			{
				if (DialogueScript* dialogue = dynamic_cast<DialogueScript*>(script.Instance.get()))
				{
					return dialogue;
				}
			}
			return nullptr;
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

		// The alpha of a UI element's image and text.
		static void SetUIAlpha(Entity entity, float alpha)
		{
			if (entity.IsValid() == false)
			{
				return;
			}
			if (entity.HasComponent<UIImageComponent>())
			{
				entity.GetComponent<UIImageComponent>().Color.a = alpha;
			}
			if (entity.HasComponent<UITextComponent>())
			{
				entity.GetComponent<UITextComponent>().Color.a = alpha;
			}
		}

		// The black over the whole view ("Fade" on the topmost canvas): 0 clear, 1 black.
		void SetFade(float alpha)
		{
			Entity fade = EntityOf(m_Fade);
			SetUIAlpha(fade, alpha);
			if (UI::IsVisible(fade) != (alpha > 0.0f))
			{
				UI::SetVisible(fade, alpha > 0.0f);
			}
		}

		Entity EntityOf(entt::entity entity) { return Entity{ entity, GetEntity().GetScene() }; }

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
			entt::entity Canvas = entt::null; // a World canvas over the player's head
			entt::entity Text = entt::null;
		};

		struct MessageUI
		{
			entt::entity Panel = entt::null;
			entt::entity Text = entt::null;
			MvpLabel Labels[2] = { MvpLabel::Count, MvpLabel::Count };
			float Time = 0.0f;
		};

		struct MenuUI
		{
			entt::entity Canvas = entt::null;
			entt::entity Row[MaxRows] = {};
			entt::entity Name[MaxRows] = {};
			entt::entity Count[MaxRows] = {};
			entt::entity DetailName = entt::null, DetailText = entt::null, Help = entt::null;
			entt::entity StatusName = entt::null, StatusHP = entt::null, StatusMP = entt::null, StatusHPFill = entt::null, StatusMPFill = entt::null;
		};

		entt::entity m_Camera = entt::null;
		entt::entity m_Player = entt::null;
		entt::entity m_Fade = entt::null;
		PromptUI m_Prompt;
		MessageUI m_Message;
		MenuUI m_Menu;
		entt::entity m_DialogueCanvas = entt::null;
		std::vector<Interactable> m_Interactables;
		entt::entity m_Current = entt::null; // what the prompt shows
		glm::vec2 m_LastPlayer{ 0.0f };
		glm::vec2 m_Facing{ 0.0f, 1.0f };
		std::vector<Item> m_Items;
		int m_Selected = 0;
		int m_NpcLine = 0;
		float m_Time = 0.0f;
		float m_Leaving = 0.0f;
		float m_Arriving = 0.0f;
		float m_Resting = 0.0f;
		std::string m_LeavingTo;
		bool m_Previous[9] = {};
	};
}
