#pragma once

int main(int argc, char** argv);

namespace Hydro
{
	class Application
	{
	public:
		Application();
		virtual ~Application();

	private:
		void Run();

	private:
		static Application* s_Instance;
		friend int ::main(int argc, char** argv);
	};

	Application* CreateApplication();
}

