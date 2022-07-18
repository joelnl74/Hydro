#pragma once

namespace Hydro
{
	class Layer
	{
	public:
		Layer();
		virtual ~Layer();

		virtual void OnAttach();
		virtual void OnDetach();
	};
}
