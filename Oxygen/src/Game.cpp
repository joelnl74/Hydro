#include "Game.h"
#include <thread>         // std::this_thread::sleep_for
#include <chrono>         // std::chrono::seconds
#include <cstdlib>		  // Random.
#include "../Hydro/src/Hydro/Core/Input.h"

namespace Hydro
{
	Game::Game()
	{
	}
	void Game::Start(Scene& scene)
	{
		// Rendering field
		for (int i = 0; i < 24; i++)
		{
			for (int j = 0; j < 18; j++)
			{
				SpriteComponent sprite;
				glm::mat4 transform = glm::mat4(1);

				transform = glm::translate(transform, glm::vec3(i * 32 + 32, j * 32 + 32, 0));
				transform = glm::scale(transform, glm::vec3(32, 32, 0));

				sprite.Color = glm::vec4(0.2, 0.2, 0.2, 1);
				sprite.Position = transform;

				board[i][j].spriteComponent = sprite;
				board[i][j].entity = '.';
				board[i][j].position = glm::vec2(i, j);

				if (i == 0 || i == 23 || j == 0 || j == 17)
				{
					sprite.Color = glm::vec4(1, 0, 0, 1);
					board[i][j].entity = '#';
				}

				scene.m_spriteComponents.push_back(&board[i][j].spriteComponent);
			}
		}

		board[(int)player.position.x][(int)player.position.y].entity = 'p';
		board[(int)food.position.x][(int)food.position.y].entity = 'f';
	}
	void Game::Update()
	{
		if (Input::IsKeyPressed(KeyCode::A))
		{
			// Change direction -1;
			player.direction = player.direction - 1 < 0 ? 3 : player.direction - 1;

		}

		if (Input::IsKeyPressed(KeyCode::D))
		{
			// Change direction +1;
			player.direction = player.direction + 1 > 3 ? 0 : player.direction + 1;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		bool isDeath = IsDeath();
		bool isFood = IsFood();

		if (isDeath)
		{
			for (int i = 0; i < 24; i++)
			{
				for (int j = 0; j < 18; j++)
				{
					if (board[i][j].entity == 's')
					{
						board[i][j].entity = '.';
					}
				}
			}

			player.parts.clear();

			board[(int)player.position.x][(int)player.position.y].entity = '.';
			player.position = glm::vec2(12, 9);
			board[(int)player.position.x][(int)player.position.y].entity = 'p';

			RenderField();
			return;
		}

		if (isFood == false)
		{
			board[(int)player.position.x][(int)player.position.y].entity = '.';
		}

		if (isFood)
		{
			PlayerPart playerPart;

			if (player.parts.size() == 0)
			{
				playerPart.position = player.position;
			}
			else
			{
				playerPart.position = player.parts[player.parts.size() - 1].position;
			}

			board[(int)playerPart.position.x][(int)playerPart.position.y].entity = 's';
			player.parts.push_back(playerPart);

			RespawnFood();
		}

		if (player.parts.size() != 0)
		{
			auto lastItemPosition = player.parts[player.parts.size() - 1].position;
			board[(int)lastItemPosition.x][(int)lastItemPosition.y].entity = '.';

			for (int i = player.parts.size() - 1; i > 0; i--)
			{
				player.parts[i].position = player.parts[i - 1].position;
				board[(int)player.parts[i].position.x][(int)player.parts[i].position.y].entity = 's';
			}

			player.parts[0].position = player.position;
			board[(int)player.position.x][(int)player.position.y].entity = 's';
		}

		switch (player.direction)
		{
		case 0:
			player.position.y += 1;
			break;
		case 1:
			player.position.x -= 1;
			break;
		case 2:
			player.position.y -= 1;
			break;
		case 3:
			player.position.x += 1;
			break;
		}

		board[(int)player.position.x][(int)player.position.y].entity = 'p';


		RenderField();
	}
	
	void Game::RenderField()
	{
		// Rendering field
		for (int i = 0; i < 24; i++)
		{
			for (int j = 0; j < 18; j++)
			{
				switch (board[i][j].entity)
				{
				case 'p':
					board[i][j].spriteComponent.Color = player.color;
					continue;
				case 's':
					board[i][j].spriteComponent.Color = player.color + 0.1f;
					continue;
				case '#':
					board[i][j].spriteComponent.Color = glm::vec4(1, 0, 0, 1);
					continue;
				case '.':
					board[i][j].spriteComponent.Color = glm::vec4(0.2, 0.2, 0.2, 1);
					continue;
				case 'f':
					board[i][j].spriteComponent.Color = glm::vec4(1, 0.5, 1, 1);
					continue;
				}
			}
		}
	}

	void Game::RespawnFood()
	{
		std::vector<glm::vec2> freePosition;

		for (int i = 0; i < 24; i++)
		{
			for (int j = 0; j < 18; j++)
			{
				if (board[i][j].entity == '.')
				{
					freePosition.push_back(glm::vec2(i, j));
				}
			}
		}
		auto ub = freePosition.size();
		auto lb = 0;
		auto random = (rand() % (ub - lb + 1)) + lb;
		auto position = freePosition[random];

		board[(int)position.x][(int)position.y].entity = 'f';
	}

	bool Game::IsDeath()
	{
		auto boardTile = board[(int)player.position.x][(int)player.position.y];

		switch (player.direction)
		{
		case 0:
			boardTile = board[(int)player.position.x][(int)player.position.y + 1];
			return boardTile.entity == '#' || boardTile.entity == 's';
		case 1:
			boardTile = board[(int)player.position.x - 1][(int)player.position.y];
			return boardTile.entity == '#' || boardTile.entity == 's';
		case 2:
			boardTile = board[(int)player.position.x][(int)player.position.y - 1];
			return boardTile.entity == '#' || boardTile.entity == 's';
		case 3:
			boardTile = board[(int)player.position.x + 1][(int)player.position.y];
			return boardTile.entity == '#' || boardTile.entity == 's';
		}

		return false;
	}

	bool Game::IsFood()
	{
		auto boardTile = board[(int)player.position.x][(int)player.position.y];

		switch (player.direction)
		{
		case 0:
			boardTile = board[(int)player.position.x][(int)player.position.y + 1];
			return boardTile.entity == 'f';
		case 1:
			boardTile = board[(int)player.position.x - 1][(int)player.position.y];
			return boardTile.entity == 'f';
			player.position.x -= 1;
		case 2:
			boardTile = board[(int)player.position.x][(int)player.position.y - 1];
			return boardTile.entity == 'f';
		case 3:
			boardTile = board[(int)player.position.x + 1][(int)player.position.y];
			return boardTile.entity == 'f';
		}

		return false;
	}
}
