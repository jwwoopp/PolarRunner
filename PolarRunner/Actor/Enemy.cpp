#include "Enemy.h"

#include <Render/Renderer.h>


Enemy::Enemy(const Craft::Vector2& position)
	: Actor("[_O_]==>", position, Craft::Color::Red)
{
}

void Enemy::Advance(float amount)
{
	distance -= amount;

	if (distance <= 0.0f)
	{
		Destroy();
	}
}

