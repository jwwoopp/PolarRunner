#include <Engine/Engine.h>
#include <Actor/Enemy.h>
#include "PlayerBullet.h"

using namespace Craft;
PlayerBullet::PlayerBullet(const Craft::Vector2& position, int direction)
	: Actor("*", position, Craft::Color::Yellow),
	direction(direction),
	xPosition(static_cast<float>(position.x)) 
{

}

void PlayerBullet::OnCollision(const std::shared_ptr<Craft::Actor>& other)
{
	if (std::dynamic_pointer_cast<Enemy>(other))
	{
		// 폭발 사운드 재생.
		Engine::Get().PlayOneShot("Explosion.wav");

		other->Destroy();
		Destroy();
	}
}

void PlayerBullet::Tick(float deltaTime)
{
	Actor::Tick(deltaTime);

	const float movement = direction * moveSpeed * deltaTime;
	xPosition += movement;
	traveledDistance += std::abs(movement);

	if (traveledDistance >= 12.0f)
	{
		ChangeImage("<#>");
	}
	else if (traveledDistance >= 5.0f)
	{
		ChangeImage("<>");
	}
	
	Vector2 newPosition = GetPosition();
	// 이미지가 커져도 그물의 중심이 이동 경로에 맞도록 보정한다.
	newPosition.x = static_cast<int>(xPosition) - GetWidth() / 2;
	SetPosition(newPosition);

	if (GetPosition().x + GetWidth() < 0
		|| GetPosition().x >= Engine::Get().GetWidth())
	{
		Destroy();
	}
}
