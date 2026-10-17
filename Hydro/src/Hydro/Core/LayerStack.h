#pragma once
#include "Hydro/Core/Layer.h"

#include <vector>

namespace Hydro
{
	// The application's layers, bottom to top: regular layers first, overlays (ImGui) after them.
	// Updated bottom-up, events delivered top-down. Owns the layers it holds.
	class LayerStack
	{
	public:
		LayerStack() = default;
		~LayerStack();

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* overlay);
		void PopLayer(Layer* layer);
		void PopOverlay(Layer* overlay);

		// Detaches and deletes every layer, top-most first. Application::Shutdown calls this
		// while the renderer still exists; the destructor does not.
		void Clear();

		std::vector<Layer*>::iterator begin() { return m_Layers.begin(); }
		std::vector<Layer*>::iterator end() { return m_Layers.end(); }

	private:
		unsigned int m_LayerInsertIndex = 0;
		std::vector<Layer*> m_Layers;
	};
}
