#include "Enemy.h"

#include <Render/Renderer.h>


Enemy::Enemy(const Craft::Vector2& position)
	: Actor("_______", position, Craft::Color::Red)
{
}

Enemy::Enemy(float distance, EnemySide side)
	: Actor("_______", Craft::Vector2::Zero, Craft::Color::Red),
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
	// Actor 위치는 7칸 피격 영역의 왼쪽 끝이고, Draw는 중앙을 기준으로 합니다.
	const int x = targetPosition.x + 3;
	const int y = targetPosition.y;

	// Left는 포문이 오른쪽(도로 방향)을, Right는 왼쪽(도로 방향)을 향하도록
	// 선체를 좌우 반전해 그립니다.
	const bool facingLeft = side == EnemySide::Right;

	if (distance > 55.0f)
	{
		Craft::Renderer::Get().Submit(facingLeft ? "<|" : "|>",
			Craft::Vector2(x, y), Craft::Color::Red, 120);
		return;
	}

	// 접근이 끝난 뒤에도 화면을 가리지 않도록 중간 크기를 유지합니다.
	Craft::Renderer::Get().Submit("|+----+", Craft::Vector2(x - 5, y - 3),
		Craft::Color::Red, 120);
	Craft::Renderer::Get().Submit(facingLeft ? "|  ! ||" : "|| !  |",
		Craft::Vector2(x - 5, y - 2), Craft::Color::Red, 120);
	Craft::Renderer::Get().Submit(facingLeft ? "<==_O_|" : "|_O_==>",
		Craft::Vector2(x - 3, y), Craft::Color::Red, 120);
	Craft::Renderer::Get().Submit(facingLeft ? "/_____\\" : "\\_____/",
		Craft::Vector2(x - 4, y + 1), Craft::Color::BrightWhite, 120);
}
