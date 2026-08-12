#include <Engine/Engine.h>
#include "PlayerBullet.h"

using namespace Craft;
PlayerBullet::PlayerBullet(const Craft::Vector2& position)
	: Actor("*", position, Craft::Color::Yellow),
	xPosition(static_cast<float>(position.x)) 
{

}

void PlayerBullet::Tick(float deltaTime)
{
	Actor::Tick(deltaTime);

	xPosition += moveSpeed * deltaTime;
	
	Vector2 newPosition = GetPosition();
	newPosition.x = static_cast<int>(xPosition);
	SetPosition(newPosition);

	if (GetPosition().x >= Engine::Get().GetWidth())
	{
		Destroy();
	}
}
