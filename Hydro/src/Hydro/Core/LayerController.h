#pragma once
#include "Layer.h"
#include "hypch.h"

namespace Hydro
{
	class LayerController
	{
	public:
		LayerController() = default;
		~LayerController();

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* overlay);
		void PopLayer(Layer* layer);
		void PopOverlay(Layer* overlay);

		std::vector<Layer*>::iterator begin() { return m_Layers.begin(); }
		std::vector<Layer*>::iterator end() { return m_Layers.end(); }

	private:
		unsigned int m_LayerInsertIndex = 0;
		std::vector<Layer*> m_Layers;
	};
}
