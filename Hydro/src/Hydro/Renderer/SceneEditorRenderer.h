#pragma once

#include "Hydro/Renderer/SceneRenderer.h"

namespace Hydro
{
	// The Editor view's lines for SceneRenderer, after the lighting: script gizmos, the Terrain tool's brush, the
	// colliders and the selection (its outline, a mesh's bounds, a light's reach).
	class SceneEditorRenderer
	{
	public:
		static void Submit(Scene& scene, const SceneRenderer::EditorOverlay& overlay, const SceneCameraBasis& camera);

	private:
		// Depth-tested 3D lines for the selection: a mesh's bounds, a light's reach or cone, the sun's direction.
		static void SubmitSelectionDebugLines(Entity selected);
		// 3D colliders and character capsules, depth tested.
		static void SubmitColliderDebugLines(Scene& scene);
	};
}
