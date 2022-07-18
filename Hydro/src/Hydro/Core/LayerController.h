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

	private:
		unsigned int m_LayerInsertIndex = 0;
		std::vector<Layer*> m_Layers;
	};
}
