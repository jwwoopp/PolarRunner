#include "EnemyBullet.h"
#include <Engine/Engine.h>

using namespace Craft;
EnemyBullet::EnemyBullet(const Craft::Vector2& position, float moveSpeed)
	: Actor("@", position, Color::Red),
	moveSpeed(moveSpeed),
	xPosition(static_cast<float>(position.x))
{
}

void EnemyBullet::Tick(float deltaTime)
{
	super::Tick(deltaTime);

	// x 위치 업데이트 (아래로 이동 처리).
	xPosition += moveSpeed * deltaTime;

	// 좌표 검사 (좌/우 어느 방향으로 발사되어도 화면 밖에서 정리됩니다).
	if (xPosition <= 0.0f || xPosition >= Engine::Get().GetWidth() - 1)
	{
		Destroy();
		return;
	}

	// 위치 설정.
	SetPosition(Vector2(
		static_cast<int>(xPosition), GetPosition().y
	));
}
