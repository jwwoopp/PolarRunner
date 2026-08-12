#pragma once

#include <Actor/Actor.h>

class EnemyBullet : public Craft::Actor
{
	// 커스텀 타입 설정.
	TYPE_DECLARATIONS(EnemyBullet, Actor)

public:
	EnemyBullet(
		const Craft::Vector2& position,
		float moveSpeed = 15.0f);

private:
	// 이벤트 함수 오버라이드.
	virtual void Tick(float deltaTime) override;

private:
	// 이동 처리를 위한 변수.
	float moveSpeed = 0.0f;

	// x위치 처리를 위한 변수.
	float xPosition = 0.0f;
};

