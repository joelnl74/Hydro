#pragma once
#include "Hydro.h"
#include "VillageScripts.h"
#include "BattleScripts.h"
#include "DialogueScripts.h"
#include "QuestScripts.h"
#include "Data/DialogueDefinition.h"
#include "Data/FieldDefinitions.h"
#include "Data/InventoryDefinition.h"
#include "Data/ItemDefinition.h"
#include "State/Inventory.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

// The MVP scene's mechanics (docs/DioramaRPG.md, the GDD's first-scene test), used by the field scenes (the
// Shrine Glade, the Farm Village) and the interiors (the Farm Village's inn): interact prompts (a sign, a chest,
// a door into a building and back out, a bed to rest in, a save point, the map's exits, an NPC) and the party's
// inventory menu (its items are data: Assets/Data/Items.hytable, the run starting from StartingInventory.hydef),
// which gates the player's input while it is open. Prototype
// code, like BattleScripts.h: one script holds it all. The UI is runtime UI under the MVP Director on the 1920 x 1080
// canvas (scripts/ui/build_game_ui.py builds it): the inventory (a scrolling list, the item's details, the party, a
// help bar), the message banner, the fade, and the "E  Talk" prompt as a World canvas over the player's head; NPCs
// talk through the dialogue box (DialogueScripts.h). What NPCs and signs say is data (Assets/Data/Npcs.hytable,
// Signs.hytable, Dialogue/*.hydef), found by the name in their tag.
namespace Hydro
{
	// The MVP's own messages and prompts (once rows of Sprites/UI/MvpText.png; the runtime UI writes their words,
	// GetMvpText): the game's UI wording, in code until there is a text table (with localisation). Content (items,
	// what NPCs and signs say) is data: Assets/Data/.
	enum class MvpLabel : uint32_t
	{
		Inventory, EnterUse, EscClose,
		PromptRead, PromptOpen, PromptEnter, PromptSave, PromptLeave, PromptTalk,
		ChestEmpty, Rested, RestedLine2, NoSaveSlot, RoadGoesOn, ToTheVillage,
		UsedPotion, NotUsableHere, NoneLeft, SignBlank,
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
		case MvpLabel::PromptRead: return "E: Read";
		case MvpLabel::PromptOpen: return "E: Open";
		case MvpLabel::PromptEnter: return "E: Enter";
		case MvpLabel::PromptSave: return "E: Save";
		case MvpLabel::PromptLeave: return "E: Leave";
		case MvpLabel::PromptTalk: return "E: Talk";
		case MvpLabel::ChestEmpty: return "The chest is empty.";
		case MvpLabel::Rested: return "You feel rested.";
		case MvpLabel::RestedLine2: return "HP and MP restored.";
		case MvpLabel::NoSaveSlot: return "No save slot yet.";
		case MvpLabel::RoadGoesOn: return "The road goes on...";
		case MvpLabel::ToTheVillage: return "To the village.";
		case MvpLabel::UsedPotion: return "Used an item.";
		case MvpLabel::NotUsableHere: return "Not usable here.";
		case MvpLabel::NoneLeft: return "None left.";
		case MvpLabel::SignBlank: return "The sign is too weathered to read.";
		default: return "";
		}
	}

	HY_SCRIPT()
	class MvpDirectorScript : public ScriptableEntity
	{
	public:
		// The Sandbox project's data files: the defaults of the properties below, so the scenes (saved before these
		// properties existed) use them without being edited.
		static constexpr uint64_t DefaultItemsTable = 4741649512266718899ull;        // Assets/Data/Items.hytable
		static constexpr uint64_t DefaultStartingInventory = 6616718844402080283ull; // Assets/Data/StartingInventory.hydef
		static constexpr uint64_t DefaultNpcTable = 7308464824897497192ull;          // Assets/Data/Npcs.hytable
		static constexpr uint64_t DefaultSignTable = 653601014895859085ull;          // Assets/Data/Signs.hytable

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

		// Items (data, the Definitions plan's D4: #171).
		HY_PROPERTY(Asset(Definition, InventoryDefinition), Tooltip("The items a run starts with, in their order"))
		AssetHandle StartingInventory = DefaultStartingInventory;
		HY_PROPERTY(DataRow(ItemDefinition), Tooltip("What a chest gives (until chests have a script of their own)"))
		DataRowRef ChestItem{ DefaultItemsTable, "Potion" };

		// What NPCs and signs say: rows by the name in their tag, without spaces ("Npc Village Elder" -> VillageElder);
		// a row "Default" for the rest.
		HY_PROPERTY(Asset(DataTable, NpcDefinition), Tooltip("The NPCs: name on the box and dialogue, by the name after \"Npc \""))
		AssetHandle NpcTable = DefaultNpcTable;
		HY_PROPERTY(Asset(DataTable, SignDefinition), Tooltip("The signs' lines, by the name after \"Sign \""))
		AssetHandle SignTable = DefaultSignTable;

		// The dialogue box (Scripting plan SC4): set it in the inspector, or drag "Dialogue Canvas" onto it. Unset,
		// the director looks for the entity tagged "Dialogue Canvas" (the scenes made before references).
		HY_PROPERTY(Tooltip("The dialogue box NPCs talk through (the Dialogue Canvas); unset: the entity tagged \"Dialogue Canvas\""))
		ComponentRef<DialogueScript> Dialogue;

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

			BattleState::Get().StartParty();

			// Everything the player can interact with, by tag prefix.
			auto view = scene.m_Registry.view<TagComponent, TransformComponent>();
			for (auto entity : view)
			{
				const std::string& tag = view.get<TagComponent>(entity).Tag;
				Interactable item;
				item.Entity = entity;
				if (tag.rfind("Sign", 0) == 0) { item.Type = Kind::Sign; item.Target = tag.size() > 5 ? tag.substr(5) : std::string(); }
				else if (tag.rfind("Chest", 0) == 0) { item.Type = Kind::Chest; }
				else if (tag.rfind("Door", 0) == 0) { item.Type = Kind::Door; item.Target = tag.size() > 5 ? tag.substr(5) : std::string(); }
				else if (tag.rfind("Rest", 0) == 0) { item.Type = Kind::Rest; }
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

			// The party's items: rows of the items table and counts, on the game session, so every field scene and interior
			// of a run shares them (what is used in the inn is gone in the village too). The first director of a run gives
			// the starting inventory (twenty items, more than the list shows, so it scrolls).
			Inventory& inventory = Inventory::Get();
			if (inventory.Started == false)
			{
				inventory.Started = true;
				const DataView<InventoryDefinition> start = Data::Get<InventoryDefinition>(StartingInventory);
				if (start.IsValid())
				{
					for (const InventoryEntry& entry : start->Items)
					{
						inventory.Slots.push_back(Slot{ entry.Item, entry.Count });
					}
				}
				HY_INFO("MVP: {} items in the inventory", inventory.Slots.size());
			}

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
					if (m_LeavingPush)
					{
						SceneTransition::Push(m_LeavingTo);
					}
					else
					{
						Leave(m_LeavingTo);
					}
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
		enum class Kind : uint8_t { Sign, Chest, Door, Rest, Save, Exit, Npc };

		struct Interactable
		{
			entt::entity Entity = entt::null;
			Kind Type = Kind::Sign;
			float FeetDrop = 0.0f; // from its position (in the world, read when needed: it may have a parent) to its feet
			bool Used = false;   // chests open once
			std::string Target;  // exits: the scene they lead to (the tag after "Exit "); doors: the building (after "Door ",
			                     // "Out" in an interior); NPCs: their name (after "Npc ")
		};

		using Slot = Inventory::Slot;

		static constexpr int MaxRows = 20; // the inventory's rows in the scene ("Item 0" to "Item 19"); more items aren't shown

		// A tag's name as a row ID: what an ID can hold ("Village Elder" -> VillageElder).
		static std::string ToRowId(const std::string& name)
		{
			std::string id;
			for (char c : name)
			{
				if (std::isalnum((unsigned char)c) != 0 || c == '_')
				{
					id += c;
				}
			}
			return id;
		}

		// The row of `table` for the entity named `name` (its tag after "Npc " / "Sign "), else its "Default" row
		// (`usedDefault` says which).
		template<typename T>
		static DataView<T> FindNamedRow(AssetHandle table, const std::string& name, bool* usedDefault = nullptr)
		{
			if (usedDefault != nullptr)
			{
				*usedDefault = false;
			}
			const DataTableView<T> rows = Data::GetTable<T>(table);
			if (rows.IsValid() == false)
			{
				return {};
			}
			const std::string id = ToRowId(name);
			if (rows.FindRow(id) != nullptr)
			{
				return Data::GetRow<T>(rows.MakeRef(id));
			}
			if (usedDefault != nullptr)
			{
				*usedDefault = true;
			}
			return Data::GetRow<T>(rows.MakeRef("Default"));
		}

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
			UI::SetText(EntityOf(m_Prompt.Text), PromptFor(*nearest));
			// Just above the player's head, in either camera view (DioramaCamera.h): the World canvas sits on that point.
			// 1.95 m (Oct 31, 2026, was 2.35): close over her hair, under the torii's shimenawa and door lintels.
			const glm::vec3 overHead = DioramaView::Above(glm::vec3(player, playerHeight), 1.95f);
			SetPosition(scene, m_Prompt.Canvas, overHead);

			if (KeyPressed(KeyCode::E, m_Previous[1]) == false || m_Resting > 0.0f)
			{
				return;
			}
			switch (nearest->Type)
			{
				case Kind::Sign:
				{
					const DataView<SignDefinition> sign = FindNamedRow<SignDefinition>(SignTable, nearest->Target);
					std::string text;
					if (sign.IsValid())
					{
						for (const std::string& line : sign->Lines)
						{
							text += (text.empty() ? "" : "\n") + line;
						}
					}
					if (text.empty())
					{
						ShowMessage(MvpLabel::SignBlank);
					}
					else
					{
						ShowMessage(text);
					}
					break;
				}
				case Kind::Chest:
					if (nearest->Used)
					{
						ShowMessage(MvpLabel::ChestEmpty);
					}
					else
					{
						nearest->Used = true;
						SetSprite(scene, nearest->Entity, 1);   // the open lid
						GiveItem(ChestItem);
					}
					break;
				case Kind::Door:
				{
					// "Door Out": back to the scene below (the one the player came in from, exactly as it was left).
					if (nearest->Target == "Out")
					{
						if (SceneTransition::GetDepth() == 0)
						{
							HY_WARN("MVP: '{}' leads out, but this scene was not entered from another (play it from the field scene)",
								scene.m_Registry.get<TagComponent>(nearest->Entity).Tag);
							break;
						}
						StartLeaving(std::string(), false);
						break;
					}
					// "Door <Name>": into the building's interior scene.
					const std::string interior = FindInteriorScene(scene, nearest->Target);
					if (interior.empty())
					{
						// No interior scene for it (yet): the door rests the party, as the inn's door did before interiors.
						HY_WARN("MVP: no interior scene for '{}' (looked for {}{} and {}); resting instead",
							scene.m_Registry.get<TagComponent>(nearest->Entity).Tag, scene.GetName(), nearest->Target, nearest->Target);
						Rest();
						break;
					}
					StartLeaving(interior, true);
					break;
				}
				case Kind::Rest:
					Rest();
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
					StartLeaving(nearest->Target, false);
					break;
				case Kind::Npc:
					// A quest may have its own words for this NPC (QuestLog::Get().Talk, QuestScripts.h).
					if (QuestLog::Get().Talk(nearest->Target))
					{
						ShowPrompt(false);
						break;
					}
					{
						// An NPC without a row of its own says the "Default" row's lines under its own (tag) name.
						bool usedDefault = false;
						const DataView<NpcDefinition> npc = FindNamedRow<NpcDefinition>(NpcTable, nearest->Target, &usedDefault);
						const DataView<DialogueDefinition> lines = npc.IsValid() ? Data::Get<DialogueDefinition>(npc->Dialogue) : DataView<DialogueDefinition>();
						const std::string name = npc.IsValid() && usedDefault == false && npc->Name.empty() == false ? npc->Name : nearest->Target;
						if (lines.IsValid() == false || lines->Lines.empty())
						{
							HY_WARN("MVP: '{}' has nothing to say (no row or dialogue for it in the NPC table)", nearest->Target);
							break;
						}
						if (DialogueScript* dialogue = FindDialogue(scene))
						{
							dialogue->Open(*lines, name);
							ShowPrompt(false);
						}
						else
						{
							// A scene without the dialogue box: one line at a time in the message box.
							ShowMessage(lines->Lines[(size_t)m_NpcLine % lines->Lines.size()].Text);
							m_NpcLine++;
						}
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

		// Fades out, then goes: Push(scene) into an interior, or Leave(scene) through an exit (and "Door Out").
		void StartLeaving(const std::string& scene, bool push)
		{
			m_LeavingTo = scene;
			m_LeavingPush = push;
			m_Leaving = LeaveTime;
			HY_INFO("MVP: {} {}", push ? "entering" : "leaving for", scene.empty() ? std::string("the scene below") : scene);
		}

		// A dip to black and back that restores the party (a bed, the inn).
		void Rest()
		{
			BattleState::Get().RestoreParty();
			m_Resting = RestTime;
			ShowMessage(MvpLabel::Rested, MvpLabel::RestedLine2);
			HY_INFO("MVP: the party rests: HP and MP restored");
		}

		// The interior scene a door leads to: "<this scene><Name>" (the Farm Village's "Door Inn" is FarmVillageInn, so
		// every village can have its own inn), else "<Name>"; empty when neither file is in the project's scenes folder.
		static std::string FindInteriorScene(Scene& scene, const std::string& name)
		{
			if (name.empty() || AssetManager::IsProjectOpen() == false)
			{
				return std::string();
			}
			const std::filesystem::path scenes = AssetManager::GetAssetDirectory() / "Scenes";
			const std::string candidates[2] = { scene.GetName() + name, name };
			for (const std::string& candidate : candidates)
			{
				std::error_code error;
				if (std::filesystem::exists(scenes / (candidate + ".hyscene"), error))
				{
					return candidate;
				}
			}
			return std::string();
		}

		static const char* PromptFor(const Interactable& item)
		{
			switch (item.Type)
			{
				case Kind::Chest: return "E  Open";
				case Kind::Door:
					if (item.Target == "Out") { return "E  Leave"; }
					if (item.Target == "Upstairs") { return "E  Go upstairs"; }
					return "E  Enter";
				case Kind::Rest: return "E  Rest";
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
			m_Message.Custom.clear();
			m_Message.Labels[0] = first;
			m_Message.Labels[1] = second;
			m_Message.Time = MessageTime;
			PresentMessage();
		}

		// A message of its own (an item's name in it), instead of the MvpLabel lines.
		void ShowMessage(const std::string& text)
		{
			m_Message.Custom = text;
			m_Message.Labels[0] = MvpLabel::Count;
			m_Message.Labels[1] = MvpLabel::Count;
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
			std::string text = m_Message.Custom.empty() ? GetMvpText(m_Message.Labels[0]) : m_Message.Custom;
			if (m_Message.Custom.empty() && m_Message.Labels[1] != MvpLabel::Count)
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
				m_Selected = std::clamp(m_Selected, 0, std::max(0, std::min((int)Inventory::Get().Slots.size(), MaxRows) - 1));
				UI::Focus(EntityOf(m_Menu.Row[m_Selected]));
			}
		}

		// One more of `item` (a chest): onto its line, or a new line at the end.
		void GiveItem(const DataRowRef& itemRef)
		{
			const DataView<ItemDefinition> item = Data::GetRow<ItemDefinition>(itemRef);
			if (item.IsValid() == false)
			{
				ShowMessage(MvpLabel::ChestEmpty); // no such item (warned once): nothing to give
				return;
			}
			auto slot = std::find_if(Inventory::Get().Slots.begin(), Inventory::Get().Slots.end(), [&](const Slot& candidate) { return candidate.Item == itemRef; });
			if (slot == Inventory::Get().Slots.end())
			{
				Inventory::Get().Slots.push_back(Slot{ itemRef, 0 });
				slot = Inventory::Get().Slots.end() - 1;
			}
			slot->Count += 1;
			ShowMessage("Found a " + item->Name + "!");
			HY_INFO("MVP: found a {} ({} now)", item->Name, slot->Count);
		}

		void UseSelected()
		{
			if (m_Selected < 0 || m_Selected >= (int)Inventory::Get().Slots.size())
			{
				return;
			}
			Slot& slot = Inventory::Get().Slots[m_Selected];
			const DataView<ItemDefinition> item = Data::GetRow<ItemDefinition>(slot.Item);
			if (item.IsValid() == false || item->CanUseInField() == false)
			{
				ShowMessage(MvpLabel::NotUsableHere);
				return;
			}
			if (slot.Count <= 0)
			{
				ShowMessage(MvpLabel::NoneLeft);
				return;
			}
			slot.Count -= 1;
			for (int h = 0; h < HeroCount; h++)
			{
				BattleState::Get().HeroMP[h] = std::min(GetHero(h).MaxMP, BattleState::Get().HeroMP[h] + item->RestoreMP);
				if (BattleState::Get().HeroHP[h] > 0)
				{
					BattleState::Get().HeroHP[h] = std::min(GetHero(h).MaxHP, BattleState::Get().HeroHP[h] + item->RestoreHP);
				}
			}
			ShowMessage(MvpLabel::UsedPotion);
			HY_INFO("MVP: used {} ({} left): {} {} HP {} MP", item->Name, slot.Count, GetHero(0).Name, BattleState::Get().HeroHP[0], BattleState::Get().HeroMP[0]);
			PresentMenu();
		}

		// The inventory's rows: one per item (rows past the list are hidden, so the panel fits it), name and count;
		// items that can't be used here are greyed, items used up dimmer still. Every row can take the focus. An
		// item whose row is gone from the table shows as "Missing: <id>".
		void PresentMenu()
		{
			for (int i = 0; i < MaxRows; i++)
			{
				const bool used = i < (int)Inventory::Get().Slots.size();
				Entity row = EntityOf(m_Menu.Row[i]);
				if (UI::IsVisible(row) != used)
				{
					UI::SetVisible(row, used);
				}
				if (used == false)
				{
					continue;
				}
				const Slot& slot = Inventory::Get().Slots[i];
				const DataView<ItemDefinition> item = Data::GetRow<ItemDefinition>(slot.Item);
				const bool usable = item.IsValid() && item->CanUseInField();
				Entity name = EntityOf(m_Menu.Name[i]);
				UI::SetText(name, item.IsValid() ? item->Name : "Missing: " + slot.Item.Row);
				UI::SetText(EntityOf(m_Menu.Count[i]), "x" + std::to_string(slot.Count));
				if (name.IsValid() && name.HasComponent<UITextComponent>())
				{
					const glm::vec3 color = usable == false ? glm::vec3(0.74f, 0.71f, 0.64f)
						: (slot.Count <= 0 ? glm::vec3(0.5f) : glm::vec3(0.95f, 0.91f, 0.83f));
					name.GetComponent<UITextComponent>().Color = glm::vec4(color, 1.0f);
				}
			}
			PresentDetails();

			// The party's status beside the list (the first hero; the MVP has one).
			const CombatantDefinition& hero = GetHero(0);
			UI::SetText(EntityOf(m_Menu.StatusName), hero.Name);
			UI::SetText(EntityOf(m_Menu.StatusHP), std::to_string(BattleState::Get().HeroHP[0]) + " / " + std::to_string(hero.MaxHP));
			UI::SetText(EntityOf(m_Menu.StatusMP), std::to_string(BattleState::Get().HeroMP[0]) + " / " + std::to_string(hero.MaxMP));
			SetBarFill(EntityOf(m_Menu.StatusHPFill), (float)BattleState::Get().HeroHP[0] / (float)hero.MaxHP);
			SetBarFill(EntityOf(m_Menu.StatusMPFill), (float)BattleState::Get().HeroMP[0] / (float)hero.MaxMP);
		}

		// The selected item's name and description, and the help line.
		void PresentDetails()
		{
			if (m_Selected < 0 || m_Selected >= (int)Inventory::Get().Slots.size())
			{
				return;
			}
			const Slot& slot = Inventory::Get().Slots[m_Selected];
			const DataView<ItemDefinition> item = Data::GetRow<ItemDefinition>(slot.Item);
			if (item.IsValid() == false)
			{
				UI::SetText(EntityOf(m_Menu.DetailName), "Missing: " + slot.Item.Row);
				UI::SetText(EntityOf(m_Menu.DetailText), "This item is not in the items table any more.");
				UI::SetText(EntityOf(m_Menu.Help), "Can't be used here.");
				return;
			}
			UI::SetText(EntityOf(m_Menu.DetailName), item->Name);
			UI::SetText(EntityOf(m_Menu.DetailText), item->Description);
			UI::SetText(EntityOf(m_Menu.Help), item->CanUseInField() ? (slot.Count > 0 ? "Use it now." : "None left.") : "Can't be used here.");
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

		// The dialogue box's script: the Dialogue property's, else the one on "Dialogue Canvas"; nullptr in a scene
		// without one.
		DialogueScript* FindDialogue(Scene& scene)
		{
			if (Dialogue.IsSet())
			{
				return Dialogue.Get();
			}
			if (Alive(scene, m_DialogueCanvas) == false)
			{
				return nullptr;
			}
			Entity canvas{ m_DialogueCanvas, &scene };
			return canvas.TryGetComponent<DialogueScript>();
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
			std::string Custom; // shown instead of the labels when set
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
		int m_Selected = 0;
		int m_NpcLine = 0;
		float m_Time = 0.0f;
		float m_Leaving = 0.0f;
		float m_Arriving = 0.0f;
		float m_Resting = 0.0f;
		std::string m_LeavingTo;
		bool m_LeavingPush = false; // into an interior (Push) rather than out through an exit (Leave)
		bool m_Previous[9] = {};
	};
}
