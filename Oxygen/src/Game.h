#pragma once
#include <glm/gtc/matrix_transform.hpp>
#include "../Hydro/src/Hydro/Scene/Components/SpriteComponent.h"
#include "../Hydro/src/Hydro/Scene/Scene.h"

namespace Hydro
{
	struct PlayerPart
	{
		glm::vec2 position;
		glm::vec4 color;

		PlayerPart()
		{
		}
	};

	struct Tile
	{
		glm::vec2 position;
		SpriteComponent spriteComponent;
		glm::vec2 color;
		char entity;
	};

	struct Player
	{
		Player()
		{
			position = glm::vec2(12, 9);
			color = glm::vec4(1, 1, 0, 0);
		}

		glm::vec2 position;
		glm::vec4 color;

		// North West, South, East
		int direction = 0;

		std::vector<PlayerPart> parts;
	};


	struct Food
	{
		glm::vec2 position;

		Food()
		{
			position = glm::vec2(6, 5);
		}
	};

	class Game
	{
	public:
		Game();

		void Start(Scene& scene);
		void Update();

	private:
		void RenderField();
		void RespawnFood();
		bool IsDeath();
		bool IsFood();

	private:
		Tile board[24][18];
		Player player = Player();
		Food food = Food();
	};
}
