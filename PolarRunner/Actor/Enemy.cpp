#include "Enemy.h"

#include <Render/Renderer.h>


Enemy::Enemy(const Craft::Vector2& position)
	: Actor("[_O_]==>", position, Craft::Color::Red)
{
}

Enemy::Enemy(float distance, EnemySide side)
	: Actor("<P>", Craft::Vector2::Zero, Craft::Color::Red),
	distance(distance),
	side(side)
{
}

void Enemy::Advance(float amount, float chaseDistance)
{
	if (state != EnemyState::ApproachingDepth)
	{
		// 플레이어가 앞뒤로 움직여도 같은 화면 줄을 유지합니다.
		distance = chaseDistance;
		return;
	}

	distance -= amount;
	if (distance <= chaseDistance)
	{
		distance = chaseDistance;
		state = EnemyState::ClosingSide;
	}
}

void Enemy::Draw()
{
	const Craft::Vector2 targetPosition = GetPosition();
	const int x = targetPosition.x;
	const int y = targetPosition.y;

	if (distance > 55.0f)
	{
		ChangeImage("<P>");
		Craft::Renderer::Get().Submit("|>", Craft::Vector2(x, y),
			Craft::Color::Red, 120);
		return;
	}

	// 접근이 끝난 뒤에도 화면을 가리지 않도록 중간 크기를 유지합니다.
	ChangeImage("[P]");
	Craft::Renderer::Get().Submit("|+----+", Craft::Vector2(x - 5, y - 3),
		Craft::Color::Red, 120);
	Craft::Renderer::Get().Submit("|| !  |", Craft::Vector2(x - 5, y - 2),
		Craft::Color::Red, 120);
	Craft::Renderer::Get().Submit("|_O_==>", Craft::Vector2(x - 3, y),
		Craft::Color::Red, 120);
	Craft::Renderer::Get().Submit("\\_____/", Craft::Vector2(x - 4, y + 1),
		Craft::Color::BrightWhite, 120);
}
