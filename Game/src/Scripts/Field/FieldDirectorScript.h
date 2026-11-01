// Game/src/Scripts/Field/FieldDirectorScript.h
#pragma once

#include "Hydro.h"
#include "Data/DialogueDefinition.h"
#include "Data/FieldDefinitions.h"
#include "Data/InventoryDefinition.h"
#include "Data/ItemDefinition.h"
#include "Rules/ItemRules.h"
#include "Scripts/Common/DioramaCamera.h"
#include "Scripts/Field/FieldMenuView.h"
#include "State/BattleState.h"
#include "State/Inventory.h"
#include "State/QuestLog.h"
#include "Views/DialogueScript.h"
#include "Views/InteractPromptView.h"
#include "Views/MessageBannerView.h"
#include "Views/ScreenFadeView.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <cctype>
#include <filesystem>

// The field director (FieldDirectorScript; MvpDirectorScript until Nov 1, 2026): the MVP scene's mechanics
// (docs/DioramaRPG.md, the GDD's first-scene test), used by the field scenes (the Shrine Glade, the Farm Village)
// and the interiors (the Farm Village's inn): interact prompts (a sign, a chest,
// a door into a building and back out, a bed to rest in, a save point, the map's exits, an NPC) and the field menu
// (Tab; its Inventory page lists the party's items, which are data: Assets/Data/Items.hytable, the run starting from
// StartingInventory.hydef), which gates the player's input while it is open. The director decides; the views show
// (Scripting plan SC8f): FieldMenuView with its InventoryView (Scripts/Field/), InteractPromptView,
// MessageBannerView and ScreenFadeView (Views/); using an item is Rules/ItemRules.h. The UI is runtime UI under the
// director ("MVP Director" in the scenes) on the 1920 x 1080 canvas (scripts/ui/build_game_ui.py builds it): the
// field menu (tabs, a page per tab, the party, a help bar), the message banner, the fade, and the "E  Talk" prompt as a World canvas over the player's head; NPCs
// talk through the dialogue box (Views/DialogueScript.h). What NPCs and signs say is data (Assets/Data/Npcs.hytable,
// Signs.hytable, Dialogue/*.hydef), found by the name in their tag.

namespace Hydro
{
	// The field's own messages and prompts (once rows of Sprites/UI/MvpText.png; the runtime UI writes their words,
	// GetFieldText): the game's UI wording, in code until there is a text table (with localisation). Content (items,
	// what NPCs and signs say) is data: Assets/Data/.
	enum class FieldLabel : uint32_t
	{
		Inventory, EnterUse, EscClose,
		PromptRead, PromptOpen, PromptEnter, PromptSave, PromptLeave, PromptTalk,
		ChestEmpty, Rested, RestedLine2, NoSaveSlot, RoadGoesOn, ToTheVillage,
		UsedPotion, NotUsableHere, NoneLeft, SignBlank,
		Count
	};

	// The words of a label, for the runtime UI.
	inline const char* GetFieldText(FieldLabel label)
	{
		switch (label)
		{
		case FieldLabel::Inventory: return "Inventory";
		case FieldLabel::EnterUse: return "Enter: use";
		case FieldLabel::EscClose: return "Esc: close";
		case FieldLabel::PromptRead: return "E: Read";
		case FieldLabel::PromptOpen: return "E: Open";
		case FieldLabel::PromptEnter: return "E: Enter";
		case FieldLabel::PromptSave: return "E: Save";
		case FieldLabel::PromptLeave: return "E: Leave";
		case FieldLabel::PromptTalk: return "E: Talk";
		case FieldLabel::ChestEmpty: return "The chest is empty.";
		case FieldLabel::Rested: return "You feel rested.";
		case FieldLabel::RestedLine2: return "HP and MP restored.";
		case FieldLabel::NoSaveSlot: return "No save slot yet.";
		case FieldLabel::RoadGoesOn: return "The road goes on...";
		case FieldLabel::ToTheVillage: return "To the village.";
		case FieldLabel::UsedPotion: return "Used an item.";
		case FieldLabel::NotUsableHere: return "Not usable here.";
		case FieldLabel::NoneLeft: return "None left.";
		case FieldLabel::SignBlank: return "The sign is too weathered to read.";
		default: return "";
		}
	}

	HY_SCRIPT()
	class FieldDirectorScript : public ScriptableEntity
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
			// The views (runtime UI on canvases under this director, so their buttons' events reach OnUIEvent).
			m_PromptView.Bind(scene);
			m_Fade.Bind(scene);
			m_Banner.Bind(scene);
			m_Menu.Bind(scene);
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
				else if (tag.rfind("Gather ", 0) == 0) { item.Type = Kind::Gather; item.Target = tag.substr(7, tag.find(' ', 7) == std::string::npos ? std::string::npos : tag.find(' ', 7) - 7); }
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
					inventory.Money = start->Money;
				}
				HY_INFO("Field: {} items in the inventory", inventory.Slots.size());
			}

			PlaceArrival(scene);
			m_Menu.SetOpen(false);
			m_PromptView.Hide();
			// Arriving (the scene was just loaded, or the player came back to it): fade in from black.
			m_Arriving = ArriveTime;
			m_Fade.Set(1.0f);
			HY_INFO("Field: WASD walk, E interact / talk (Enter next line, Esc close), Tab menu (Up/Down tabs, Enter open, Esc back / close), 1/2/3 day-dusk-night");
		}

		void OnUpdate(float deltaTime) override
		{
			Scene& scene = *GetEntity().GetScene();
			m_Time += deltaTime;

			// a scene change in progress: fade, then go
			if (m_Leaving > 0.0f)
			{
				m_Leaving = std::max(0.0f, m_Leaving - deltaTime);
				m_Fade.Set(1.0f - m_Leaving / LeaveTime);
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
				m_Fade.Set(m_Arriving / ArriveTime);
			}
			// a rest at the inn: a dip to black and back
			if (m_Resting > 0.0f)
			{
				m_Resting = std::max(0.0f, m_Resting - deltaTime);
				const float k = m_Resting / RestTime;
				m_Fade.Set(k > 0.5f ? (1.0f - k) * 2.0f : k * 2.0f);
			}

			// A runtime UI canvas that captures input (UI::CapturesInput: the menu, the dialogue, any other) owns the
			// keys: no interaction. Tab is the menu's own key, so it also closes it.
			const bool menuOpen = m_Menu.IsOpen();
			if (KeyPressed(KeyCode::Tab, m_Previous[0]) && (menuOpen || UI::CapturesInput(scene) == false))
			{
				OpenMenu(menuOpen == false);
			}
			// Q / E in the menu: the hero its Character, Abilities and Equipment pages show (polled every frame for
			// the edge; E's interaction never runs while the menu captures the keys).
			const bool previousHero = KeyPressed(KeyCode::Q, m_Previous[2]);
			const bool nextHero = KeyPressed(KeyCode::E, m_Previous[3]);
			if (m_Menu.IsOpen() && (previousHero || nextHero))
			{
				m_Menu.CycleHero(nextHero ? 1 : -1);
			}
			if (UI::CapturesInput(scene))
			{
				m_PromptView.Hide();
			}
			else
			{
				UpdateInteraction(scene);
			}

			m_Banner.Update(deltaTime);
		}

		// The field menu's tabs and rows bubble their events up to the director; the view says what the player asked
		// for: use the selected item, or close.
		bool OnUIEvent(const UIEvent& event) override
		{
			FieldMenuView::Request request = FieldMenuView::Request::None;
			if (m_Menu.HandleEvent(event, request) == false)
			{
				return false;
			}
			if (request == FieldMenuView::Request::UseItem)
			{
				UseSelected();
			}
			else if (request == FieldMenuView::Request::Close)
			{
				OpenMenu(false);
			}
			return true;
		}

	private:
		enum class Kind : uint8_t { Sign, Chest, Door, Rest, Save, Exit, Npc, Gather };

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

			if (nearest == nullptr)
			{
				m_PromptView.Hide();
				return;
			}
			// Just above the player's head, in either camera view (DioramaCamera.h): the World canvas sits on that point.
			// 1.95 m (Oct 31, 2026, was 2.35): close over her hair, under the torii's shimenawa and door lintels.
			const glm::vec3 overHead = DioramaView::Above(glm::vec3(player, playerHeight), 1.95f);
			m_PromptView.Show(PromptFor(*nearest), overHead);

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
						ShowMessage(FieldLabel::SignBlank);
					}
					else
					{
						ShowMessage(text);
					}
					break;
				}
				case Kind::Gather:
				{
					// "Gather <ItemRow> ...": one of that row of the items table, then the spot is gone (until the scene
					// is loaded again): herbs, bamboo shoots, side quests' Collect items.
					GiveItem(DataRowRef{ ChestItem.Table, nearest->Target });
					scene.DestroyEntity(Entity{ nearest->Entity, &scene });
					m_PromptView.Hide();
					break;
				}
				case Kind::Chest:
					if (nearest->Used)
					{
						ShowMessage(FieldLabel::ChestEmpty);
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
							HY_WARN("Field: '{}' leads out, but this scene was not entered from another (play it from the field scene)",
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
						HY_WARN("Field: no interior scene for '{}' (looked for {}{} and {}); resting instead",
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
					ShowMessage(FieldLabel::NoSaveSlot);
					HY_INFO("Field: save point: saving is not implemented yet (DioramaRPG.md J5)");
					break;
				case Kind::Exit:
					if (nearest->Target.empty())
					{
						ShowMessage(FieldLabel::RoadGoesOn);
						break;
					}
					ShowMessage(FieldLabel::RoadGoesOn, nearest->Target == "FarmVillage" ? FieldLabel::ToTheVillage : FieldLabel::Count);
					StartLeaving(nearest->Target, false);
					break;
				case Kind::Npc:
					// A quest may have its own words for this NPC (QuestLog::Get().Talk, State/QuestLog.h).
					if (QuestLog::Get().Talk(nearest->Target))
					{
						m_PromptView.Hide();
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
							HY_WARN("Field: '{}' has nothing to say (no row or dialogue for it in the NPC table)", nearest->Target);
							break;
						}
						if (DialogueScript* dialogue = FindDialogue(scene))
						{
							dialogue->Open(*lines, name);
							m_PromptView.Hide();
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

		// An exit's tag names the scene it leads to ("Exit JapaneseCountryside"). Going back to the scene below on the
		// stack pops (it is exactly as the player left it, at that exit); any other scene is pushed on top, so the
		// field scenes can form a line (the Shrine Glade, the countryside, the city) walked both ways. "Door Out" (no
		// target) always pops. A world map or scene graph replaces this when the world stops being a line.
		static void Leave(const std::string& target)
		{
			if (SceneTransition::GetDepth() > 0 && (target.empty() || target == SceneTransition::GetBelowName()))
			{
				SceneTransition::Pop();
			}
			else
			{
				SceneTransition::Push(target);
			}
		}

		// A scene pushed from another starts with the player at "Arrive <that scene>" when it has one (an empty
		// entity just inside the exit back), instead of where the scene was saved.
		void PlaceArrival(Scene& scene)
		{
			const std::string& from = SceneTransition::GetBelowName();
			if (from.empty() || Alive(scene, m_Player) == false)
			{
				return;
			}
			Entity marker = scene.FindEntityByTag("Arrive " + from);
			if (marker.IsValid() == false)
			{
				return;
			}
			Entity player{ m_Player, &scene };
			const glm::vec3 at = scene.GetWorldPosition(marker);
			const glm::vec3 now = scene.GetWorldPosition(player);
			scene.SetWorldPosition(player, glm::vec3(at.x, at.y, now.z));
			HY_INFO("Field: arrived from {} at ({:.1f}, {:.1f})", from, at.x, at.y);
		}

		// Fades out, then goes: Push(scene) into an interior, or Leave(scene) through an exit (and "Door Out").
		void StartLeaving(const std::string& scene, bool push)
		{
			m_LeavingTo = scene;
			m_LeavingPush = push;
			m_Leaving = LeaveTime;
			HY_INFO("Field: {} {}", push ? "entering" : "leaving for", scene.empty() ? std::string("the scene below") : scene);
		}

		// A dip to black and back that restores the party (a bed, the inn).
		void Rest()
		{
			BattleState::Get().RestoreParty();
			m_Resting = RestTime;
			ShowMessage(FieldLabel::Rested, FieldLabel::RestedLine2);
			HY_INFO("Field: the party rests: HP and MP restored");
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
				case Kind::Gather: return "E  Gather";
				default: return "E  Read";
			}
		}

		// ---- Messages ------------------------------------------------------------------------------------

		void ShowMessage(FieldLabel first, FieldLabel second = FieldLabel::Count)
		{
			std::string text = GetFieldText(first);
			if (second != FieldLabel::Count)
			{
				text += std::string("\n") + GetFieldText(second);
			}
			m_Banner.Show(text, MessageTime);
		}

		// A message of its own (an item's name in it), instead of the FieldLabel lines.
		void ShowMessage(const std::string& text)
		{
			m_Banner.Show(text, MessageTime);
		}

		// ---- The field menu --------------------------------------------------------------------------------

		// Opens or closes it; open, the prompt hides.
		void OpenMenu(bool open)
		{
			if (open)
			{
				m_PromptView.Hide();
			}
			m_Menu.SetOpen(open);
		}

		static bool IsVowel(const std::string& word)
		{
			return word.empty() == false && std::string("AEIOUaeiou").find(word[0]) != std::string::npos;
		}

		// One more of `item` (a chest, a gathering spot): onto its line, or a new line at the end.
		void GiveItem(const DataRowRef& itemRef)
		{
			const DataView<ItemDefinition> item = Data::GetRow<ItemDefinition>(itemRef);
			if (item.IsValid() == false)
			{
				ShowMessage(FieldLabel::ChestEmpty); // no such item (warned once): nothing to give
				return;
			}
			const int now = Inventory::Get().Add(itemRef, 1);
			ShowMessage(std::string(IsVowel(item->Name) ? "Found an " : "Found a ") + item->Name + "!");
			HY_INFO("Field: found a {} ({} now)", item->Name, now);
		}

		void UseSelected()
		{
			const int selected = m_Menu.Items().GetSelected();
			if (selected < 0 || selected >= (int)Inventory::Get().Slots.size())
			{
				return;
			}
			Inventory::Slot& slot = Inventory::Get().Slots[(size_t)selected];
			const DataView<ItemDefinition> item = Data::GetRow<ItemDefinition>(slot.Item);
			if (item.IsValid() == false)
			{
				ShowMessage(FieldLabel::NotUsableHere);
				return;
			}
			switch (ItemRules::UseInField(slot, *item, BattleState::Get()))
			{
				case ItemRules::UseResult::NotUsableHere:
					ShowMessage(FieldLabel::NotUsableHere);
					return;
				case ItemRules::UseResult::NoneLeft:
					ShowMessage(FieldLabel::NoneLeft);
					return;
				case ItemRules::UseResult::Used:
					break;
			}
			ShowMessage(FieldLabel::UsedPotion);
			HY_INFO("Field: used {} ({} left): {} {} HP {} MP", item->Name, slot.Count, GetHero(0).Name, BattleState::Get().HeroHP[0], BattleState::Get().HeroMP[0]);
			m_Menu.Present();
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
			HY_WARN("Field: the scene has no entity tagged \"{}\"", tag);
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

		Entity EntityOf(entt::entity entity) { return Entity{ entity, GetEntity().GetScene() }; }

		static void SetSprite(Scene& scene, entt::entity entity, uint32_t index)
		{
			if (Alive(scene, entity) && scene.m_Registry.has<SpriteRendererComponent>(entity))
			{
				scene.m_Registry.get<SpriteRendererComponent>(entity).SpriteIndex = index;
			}
		}

	private:
		entt::entity m_Camera = entt::null;
		entt::entity m_Player = entt::null;
		InteractPromptView m_PromptView;
		ScreenFadeView m_Fade;
		MessageBannerView m_Banner;
		FieldMenuView m_Menu;
		entt::entity m_DialogueCanvas = entt::null;
		std::vector<Interactable> m_Interactables;
		entt::entity m_Current = entt::null; // what the prompt shows
		glm::vec2 m_LastPlayer{ 0.0f };
		glm::vec2 m_Facing{ 0.0f, 1.0f };
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
