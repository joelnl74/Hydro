#pragma once

#include "Hydro.h"
#include "Data/DialogueDefinition.h"
#include "Data/FieldDefinitions.h"
#include "Data/InventoryDefinition.h"
#include "Data/ItemDefinition.h"
#include "Data/LootDefinition.h"
#include "Rules/ItemRules.h"
#include "Rules/LootRules.h"
#include "Scripts/Common/DioramaCamera.h"
#include "Scripts/Field/FieldMenuView.h"
#include "State/BattleState.h"
#include "State/PartyState.h"
#include "State/Inventory.h"
#include "State/QuestLog.h"
#include "State/SaveGame.h"
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
#include <random>

// The field director: interact prompts (signs, chests, doors, beds, save points, exits, NPCs) and the field menu in the
// field scenes and interiors. It decides; the views show (FieldMenuView, InteractPromptView, MessageBannerView, ScreenFadeView).

namespace Hydro
{
	// The field's own UI wording, in code until there is a text table. Content (items, NPC and sign lines) is data.
	enum class FieldLabel : uint32_t
	{
		Inventory, EnterUse, EscClose,
		PromptRead, PromptOpen, PromptEnter, PromptSave, PromptLeave, PromptTalk,
		ChestEmpty, Rested, RestedLine2, Saved, SaveFailed, NothingToLoad, RoadGoesOn, ToTheVillage,
		UsedPotion, NotUsableHere, NoneLeft, SignBlank,
		Count
	};

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
		case FieldLabel::Saved: return "Progress saved.";
		case FieldLabel::SaveFailed: return "The game could not be saved.";
		case FieldLabel::NothingToLoad: return "There is no save to load.";
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
		// Defaults of the data properties below, so scenes saved before the properties existed use them unedited.
		static constexpr uint64_t DefaultItemsTable = 4741649512266718899ull;        // Assets/Data/Items.hytable
		static constexpr uint64_t DefaultStartingInventory = 6616718844402080283ull; // Assets/Data/StartingInventory.hydef
		static constexpr uint64_t DefaultNpcTable = 7308464824897497192ull;          // Assets/Data/Npcs.hytable
		static constexpr uint64_t DefaultSignTable = 653601014895859085ull;          // Assets/Data/Signs.hytable
		static constexpr uint64_t DefaultLootTable = 207747700610060598ull;          // Assets/Data/Loot.hytable
		static constexpr uint64_t DefaultFieldUI = 1324405221109369128ull;           // Assets/Prefabs/UI/FieldUI.hyprefab

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

		// Seconds.
		HY_PROPERTY(Range(0.1, 30), Tooltip("How long a message stays up"))
		float MessageTime = 3.0f;
		HY_PROPERTY(Range(0.05, 5), Tooltip("Fade out when leaving through an exit"))
		float LeaveTime = 0.6f;
		HY_PROPERTY(Range(0.05, 5), Tooltip("Fade in after arriving"))
		float ArriveTime = 0.6f;
		HY_PROPERTY(Range(0.05, 10), Tooltip("Fade out and in when resting at a save point"))
		float RestTime = 1.2f;

		HY_PROPERTY(Asset(Definition, InventoryDefinition), Tooltip("The items a run starts with, in their order"))
		AssetHandle StartingInventory = DefaultStartingInventory;
		HY_PROPERTY(DataRow(ItemDefinition), Tooltip("What a chest gives when its tag names no loot table (a plain \"Chest\")"))
		DataRowRef ChestItem{ DefaultItemsTable, "Potion" };
		HY_PROPERTY(Asset(DataTable, LootTableDefinition), Tooltip("The loot tables chests roll: \"Chest <row> ...\" rolls that row (items and aster)"))
		AssetHandle LootTable = DefaultLootTable;

		// NPC and sign rows are found by the name in their tag without spaces ("Npc Village Elder" -> VillageElder), else "Default".
		HY_PROPERTY(Asset(DataTable, NpcDefinition), Tooltip("The NPCs: name on the box and dialogue, by the name after \"Npc \""))
		AssetHandle NpcTable = DefaultNpcTable;
		HY_PROPERTY(Asset(DataTable, SignDefinition), Tooltip("The signs' lines, by the name after \"Sign \""))
		AssetHandle SignTable = DefaultSignTable;

		HY_PROPERTY(Tooltip("The dialogue box NPCs talk through (the Dialogue Canvas); unset: the entity tagged \"Dialogue Canvas\""))
		ComponentRef<DialogueScript> Dialogue;

		// Scenes built before the prefab have the field UI baked in under this director; any other scene gets a copy at start,
		// so the UI is there whichever scene a run starts in.
		HY_PROPERTY(Asset(Prefab), Tooltip("The field UI, instantiated under this director when the scene has none of its own (no \"Field Menu Canvas\" or \"Field UI\" child)"))
		AssetHandle FieldUI = DefaultFieldUI;

	protected:
		void OnCreate() override
		{
			Scene& scene = *GetEntity().GetScene();
			m_Camera = Find(scene, "Camera");
			m_Player = Find(scene, "Player");
			EnsureFieldUI();
			// The views are runtime UI on canvases under this director, so their buttons' events reach OnUIEvent.
			m_PromptView.Bind(scene);
			m_Fade.Bind(scene);
			m_Banner.Bind(scene);
			m_Menu.Bind(scene);
			m_DialogueCanvas = Find(scene, "Dialogue Canvas");

			PartyState::Get().Start();

			auto view = scene.m_Registry.view<TagComponent, TransformComponent>();
			for (auto entity : view)
			{
				const std::string& tag = view.get<TagComponent>(entity).Tag;
				Interactable item;
				item.Entity = entity;
				if (tag.rfind("Sign", 0) == 0) { item.Type = Kind::Sign; item.Target = tag.size() > 5 ? tag.substr(5) : std::string(); }
				else if (tag.rfind("Chest", 0) == 0) { item.Type = Kind::Chest; item.Target = FirstWord(tag, 6); }
				else if (tag.rfind("Door", 0) == 0) { item.Type = Kind::Door; item.Target = tag.size() > 5 ? tag.substr(5) : std::string(); }
				else if (tag.rfind("Rest", 0) == 0) { item.Type = Kind::Rest; }
				else if (tag.rfind("Save", 0) == 0) { item.Type = Kind::Save; }
				else if (tag.rfind("Exit", 0) == 0) { item.Type = Kind::Exit; item.Target = tag.size() > 5 ? tag.substr(5) : std::string(); }
				else if (tag.rfind("Npc", 0) == 0) { item.Type = Kind::Npc; item.Target = tag.size() > 4 ? tag.substr(4) : std::string(); }
				else if (tag.rfind("Gather ", 0) == 0) { item.Type = Kind::Gather; item.Target = FirstWord(tag, 7); }
				else { continue; }
				if (scene.m_Registry.has<SpriteRendererComponent>(entity))
				{
					// A sprite's entity sits at its middle; the player talks to its feet.
					item.FeetDrop = 0.5f * view.get<TransformComponent>(entity).Scale.y;
				}
				// Found already this run: a chest stays open, a picked-up spot stays gone.
				if ((item.Type == Kind::Chest || item.Type == Kind::Gather) && Inventory::Get().WasFound(FoundKey(scene, tag)))
				{
					if (item.Type == Kind::Gather)
					{
						m_Gone.push_back(entity);
						continue;
					}
					item.Used = true;
					SetSprite(scene, entity, 1);
				}
				m_Interactables.push_back(item);
			}
			for (entt::entity gone : m_Gone)
			{
				scene.DestroyEntity(Entity{ gone, &scene });
			}
			m_Gone.clear();

			// The inventory lives on the game session, so every scene of a run shares it. The first director of a run gives the
			// starting inventory.
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

			glm::vec3 saved{ 0.0f };
			if (SaveGame::TakeArrival(scene.GetName(), saved) && Alive(scene, m_Player))
			{
				scene.SetWorldPosition(Entity{ m_Player, &scene }, saved); // just loaded: where the save was made
			}
			else
			{
				PlaceArrival(scene);
			}
			m_Menu.SetOpen(false);
			m_PromptView.Hide();
			// Arriving (just loaded, or the player came back): fade in from black.
			m_Arriving = ArriveTime;
			m_Fade.Set(1.0f);
			HY_INFO("Field: WASD walk, E interact / talk (Enter next line, Esc close), Tab menu (Up/Down tabs, Enter open, Esc back / close), 1/2/3 day-dusk-night");
		}

		void OnUpdate(float deltaTime) override
		{
			Scene& scene = *GetEntity().GetScene();
			m_Time += deltaTime;

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
					// Kept while the next scene runs, so coming back here fades in.
					m_Arriving = ArriveTime;
				}
				return;
			}
			if (m_Arriving > 0.0f)
			{
				m_Arriving = std::max(0.0f, m_Arriving - deltaTime);
				m_Fade.Set(m_Arriving / ArriveTime);
			}
			if (m_Resting > 0.0f)
			{
				m_Resting = std::max(0.0f, m_Resting - deltaTime);
				const float k = m_Resting / RestTime;
				m_Fade.Set(k > 0.5f ? (1.0f - k) * 2.0f : k * 2.0f);
			}

			// A UI canvas that captures input owns the keys: no interaction. Tab is the menu's own key, so it also closes it.
			const bool menuOpen = m_Menu.IsOpen();
			if (KeyPressed(KeyCode::Tab, m_Previous[0]) && (menuOpen || UI::CapturesInput(scene) == false))
			{
				OpenMenu(menuOpen == false);
			}
			// Polled every frame for the edge; E's interaction never runs while the menu captures the keys.
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
			else if (request == FieldMenuView::Request::Save)
			{
				const bool saved = SaveHere();
				m_Menu.Present();
				m_Menu.System().SetNotice(GetFieldText(saved ? FieldLabel::Saved : FieldLabel::SaveFailed));
			}
			else if (request == FieldMenuView::Request::Load)
			{
				LoadSave();
			}
			else if (request == FieldMenuView::Request::Restart)
			{
				OpenMenu(false); // a new run: a fresh session and the scene the run started in
				SceneTransition::Restart();
			}
			return true;
		}

	private:
		enum class Kind : uint8_t { Sign, Chest, Door, Rest, Save, Exit, Npc, Gather };

		struct Interactable
		{
			entt::entity Entity = entt::null;
			Kind Type = Kind::Sign;
			float FeetDrop = 0.0f; // from its world position to its feet
			bool Used = false;   // chests open once
			std::string Target;  // exits: the scene they lead to; doors: the building ("Out" in an interior); NPCs: their name
		};

		using Slot = Inventory::Slot;

		// "Village Elder" -> VillageElder.
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

		// The row of `table` for the entity named `name`, else its "Default" row (`usedDefault` says which).
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
			const float playerHeight = Height(scene, m_Player);

			// Where the player faces: the way it last walked.
			const glm::vec2 walked = player - m_LastPlayer;
			if (glm::dot(walked, walked) > 1.0e-6f)
			{
				m_Facing = glm::normalize(walked);
			}
			m_LastPlayer = player;

			std::vector<Entity> inside;
			Physics3D::OverlapSphere(scene, glm::vec3(player, playerHeight + 0.5f), 0.3f, Physics3D::LayerBit(PhysicsLayer::Trigger),
				inside);

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
			// Just above the player's head in either camera view: close over her hair, under the torii's shimenawa and door lintels.
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
					// "Gather <ItemRow> ...": one of that item, then the spot is gone until the scene is loaded again.
					Inventory::Get().MarkFound(FoundKey(scene, scene.m_Registry.get<TagComponent>(nearest->Entity).Tag));
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
						Inventory::Get().MarkFound(FoundKey(scene, scene.m_Registry.get<TagComponent>(nearest->Entity).Tag));
						OpenChest(nearest->Target);
					}
					break;
				case Kind::Door:
				{
					// "Door Out": back to the scene below, exactly as it was left.
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
					const std::string interior = FindInteriorScene(scene, nearest->Target);
					if (interior.empty())
					{
						// No interior scene for it: the door rests the party instead.
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
					ShowMessage(SaveHere() ? FieldLabel::Saved : FieldLabel::SaveFailed);
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
					// A quest may have its own words for this NPC.
					if (QuestLog::Get().Talk(nearest->Target))
					{
						m_PromptView.Hide();
						break;
					}
					{
						// An NPC without a row of its own says the "Default" row's lines under its own (tag) name.
						bool usedDefault = false;
						const DataView<NpcDefinition> npc = FindNamedRow<NpcDefinition>(NpcTable, nearest->Target, &usedDefault);
						const DataView<DialogueDefinition> lines = npc.IsValid() ? Data::Get<DialogueDefinition>(npc->Dialogue)
							: DataView<DialogueDefinition>();
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

		// An exit with a trigger is in reach while the player stands in it. Anything else must be within InteractRange, at about
		// the player's height, in front (unless very close) and in sight. `distance` lets the prompt pick the nearest.
		bool InReach(Scene& scene, const Interactable& item, const glm::vec2& player, float playerHeight, const std::vector<Entity>& inside,
			float& distance) const
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

		// Going back to the scene below on the stack pops it (exactly as left); any other scene is pushed on top, so the field
		// scenes form a line walked both ways. "Door Out" (no target) always pops.
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

		// A pushed scene starts the player at "Arrive <previous scene>" when it has one, instead of where the scene was saved.
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
			// On a terrain the two ends of a scene can be metres apart in height, so she takes the marker's height too.
			Entity player{ m_Player, &scene };
			const glm::vec3 at = scene.GetWorldPosition(marker);
			scene.SetWorldPosition(player, at);
			HY_INFO("Field: arrived from {} at ({:.1f}, {:.1f})", from, at.x, at.y);
		}

		// Fades out, then pushes an interior or leaves through an exit.
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
			PartyState::Get().Restore();
			m_Resting = RestTime;
			ShowMessage(FieldLabel::Rested, FieldLabel::RestedLine2);
			HY_INFO("Field: the party rests: HP and MP restored");
		}

		// "<this scene><Name>" (the Farm Village's "Door Inn" is FarmVillageInn), else "<Name>"; empty when neither exists.
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

		void ShowMessage(const std::string& text)
		{
			m_Banner.Show(text, MessageTime);
		}

		// ---- The field menu --------------------------------------------------------------------------------

		// The prompt hides while the menu is open.
		// The run's state and where the player stands, to the save slot (SaveGame: game code, YAML).
		bool SaveHere()
		{
			Scene& scene = *GetEntity().GetScene();
			SavePlace place;
			place.Scene = scene.GetName();
			if (Alive(scene, m_Player))
			{
				place.Position = scene.GetWorldPosition(Entity{ m_Player, &scene });
			}
			return SaveGame::Write(place);
		}

		// The saved state replaces the session's, then the saved scene replaces every scene; its director puts the player
		// where the save was made.
		void LoadSave()
		{
			SavePlace place;
			if (SaveGame::Load(1, place) == false)
			{
				m_Menu.System().SetNotice(GetFieldText(FieldLabel::NothingToLoad));
				return;
			}
			OpenMenu(false);
			SceneTransition::Replace(place.Scene);
		}

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

		// Adds to the item's line, or a new line at the end.
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

		// Its loot table row ("Chest <row>" in Loot.hytable), else ChestItem.
		void OpenChest(const std::string& row)
		{
			// FindRow, not GetRow: a chest named after its place ("Chest Barn") has no row and quietly gives ChestItem.
			const DataTableView<LootTableDefinition> tables = Data::GetTable<LootTableDefinition>(LootTable);
			const LootTableDefinition* table = (row.empty() || tables.IsValid() == false) ? nullptr : tables.FindRow(row);
			if (table == nullptr)
			{
				GiveItem(ChestItem);
				return;
			}
			const std::function<float()> random = [this]() { return std::uniform_real_distribution<float>(0.0f, 0.9999f)(m_Random); };
			std::string found;
			for (const LootRules::Drop& drop : LootRules::Roll(*table, random))
			{
				const DataView<ItemDefinition> item = Data::GetRow<ItemDefinition>(drop.Item);
				if (item.IsValid() == false)
				{
					continue;
				}
				Inventory::Get().Add(drop.Item, drop.Count);
				found += (found.empty() ? "" : ", ") + item->Name + (drop.Count > 1 ? " x" + std::to_string(drop.Count) : std::string());
			}
			const int money = LootRules::RollMoney(*table, random);
			if (money > 0)
			{
				Inventory::Get().Money += money;
				found += (found.empty() ? "" : " and ") + std::to_string(money) + " " + CurrencyName;
			}
			if (found.empty())
			{
				ShowMessage(FieldLabel::ChestEmpty);
				return;
			}
			ShowMessage("Found " + found + "!");
			HY_INFO("Field: chest '{}' gave {}", row, found);
		}

		// The run's key for something found in this scene (Inventory::Found).
		static std::string FoundKey(const Scene& scene, const std::string& tag)
		{
			return scene.GetName() + "/" + tag;
		}

		// Up to the next space.
		static std::string FirstWord(const std::string& tag, size_t from)
		{
			if (tag.size() <= from)
			{
				return std::string();
			}
			const size_t space = tag.find(' ', from);
			return tag.substr(from, space == std::string::npos ? std::string::npos : space - from);
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
			switch (ItemRules::UseInField(slot, *item, PartyState::Get()))
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
			HY_INFO("Field: used {} ({} left)", item->Name, slot.Count);
			m_Menu.Present();
		}

		// A copy of the FieldUI prefab as this director's last child, so the UI's events reach OnUIEvent.
		void EnsureFieldUI()
		{
			Entity self = GetEntity();
			if ((bool)self.FindChild("Field Menu Canvas") || (bool)self.FindChild("Field UI") || (uint64_t)FieldUI == 0)
			{
				return;
			}
			const Entity ui = PrefabSerializer::Instantiate(*self.GetScene(), FieldUI, self);
			if ((bool)ui)
			{
				HY_INFO("Field: the scene has no field UI; instantiated the FieldUI prefab under {}", self.GetName());
			}
			else
			{
				HY_WARN("Field: the scene has no field UI and the FieldUI prefab ({}) could not be instantiated", (uint64_t)FieldUI);
			}
		}

		// The Dialogue property's script, else the one on "Dialogue Canvas"; nullptr when the scene has none.
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

		// World positions, so these entities may sit under any parent.
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
		std::vector<entt::entity> m_Gone;          // picked-up spots found again on load (destroyed after the scan)
		std::mt19937 m_Random{ std::random_device{}() }; // chests' rolls
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
		bool m_LeavingPush = false; // into an interior rather than out through an exit
		bool m_Previous[9] = {};
	};
}
