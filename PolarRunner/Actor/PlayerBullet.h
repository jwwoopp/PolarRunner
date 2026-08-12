#pragma once

#include <Actor/Actor.h>

// 플레이어가 발사하는 탄약 클래스.
class PlayerBullet : public Craft::Actor
{
	// 커스텀 타입 설정.
	TYPE_DECLARATIONS(PlayerBullet, Actor)

public:
	PlayerBullet(const Craft::Vector2& position);

private:
	// 이벤트 오버라이딩.
	virtual void Tick(float deltaTime) override;

private:
	// 이동 속도 (빠르기-단위: 초).
	float moveSpeed = 30.0f;

	// 위치 갱신을 할 때 사용할 변수.
	float xPosition = 0.0f;
};

