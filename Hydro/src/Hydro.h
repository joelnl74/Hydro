#pragma once

// CORE
#include "Hydro/Core/Application.h"
#include "Hydro/Core/Input.h"
#include "Hydro/Core/Time.h"
#include "Hydro/Core/Layer.h"

// EVENTS.
#include "Hydro/Events/Event.h"

#include "Hydro/Events/ApplicationEvents.h"
#include "Hydro/Events/KeyEvents.h"
#include "Hydro/Events/MouseEvents.h"
#include "Hydro/Events/EventDispatcher.h"
#include "Hydro/ImGUI/ImGuiLayer.h"

// ECS.
#include "Hydro/Scene/Entity.h"
#include "Hydro/Scene/Components/Components.h"
#include "Hydro/Scene/Scene.h"
#include "Hydro/Scene/SceneSerializer.h"
#include "Hydro/Scene/SceneTransition.h"
#include "Hydro/Scene/PrefabSerializer.h"
#include "Hydro/Scene/SpriteAnimation.h"
#include "Hydro/Scene/SpriteAnimationSerializer.h"
#include "Hydro/Scene/SpriteAnimator.h"
#include "Hydro/Scene/ParticleScene.h"
#include "Hydro/Scene/Prefab.h"
#include "Hydro/Scene/Vfx.h"

// Scripting
#include "Hydro/Scene/Scripting/ScriptableEntity.h"
#include "Hydro/Scene/Scripting/ReflectionMacros.h"

// PHYSICS.
#include "Hydro/Physics/PhysicsScene.h"
#include "Hydro/Physics/Physics3D.h"

// ASSETS.
#include "Hydro/Asset/Asset.h"
#include "Hydro/Asset/AssetMetadata.h"
#include "Hydro/Asset/AssetRegistry.h"
#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Asset/AssetMetaSerializer.h"

// RENDERING.
#include "Hydro/Renderer/SceneRenderer.h"
#include "Hydro/Renderer/Camera.h"
#include "Hydro/Renderer/SceneCamera.h"
#include "Hydro/Renderer/EditorCamera.h"
#include "Hydro/Renderer/RenderView.h"
#include "Hydro/Renderer/Renderer2D.h"
#include "Hydro/Renderer/Texture2D.h"
#include "Hydro/Renderer/Material.h"
#include "Hydro/Renderer/RenderProfile.h"
#include "Hydro/Renderer/Mesh.h"
#include "Hydro/Renderer/TextureImport.h"
#include "Hydro/Renderer/TextureImportSerializer.h"
#include "Hydro/Renderer/SpriteSlicer.h"
#include "Hydro/Renderer/Renderer.h"
#include "Hydro/Renderer/RendererSettings.h"

// PROJECT.
#include "Hydro/Project/Project.h"
#include "Hydro/Project/ProjectSerializer.h"

