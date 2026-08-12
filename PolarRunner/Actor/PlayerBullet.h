#pragma once

#include <Actor/Actor.h>

// 플레이어가 발사하는 탄약 클래스.
class PlayerBullet : public Craft::Actor
{
	// 커스텀 타입 설정.
	TYPE_DECLARATIONS(PlayerBullet, Actor)

public:
	PlayerBullet(const Craft::Vector2& position, int direction);
	virtual void OnCollision(
		const std::shared_ptr<Craft::Actor>& other) override;

private:
	// 이벤트 오버라이딩.
	virtual void Tick(float deltaTime) override;

private:
	int direction = 1;

	// 이동 속도 (빠르기-단위: 초).
	float moveSpeed = 30.0f;

	// 위치 갱신을 할 때 사용할 변수.
	float xPosition = 0.0f;

	// 발사 후 이동한 거리. 그물이 펼쳐지는 단계를 결정한다.
	float traveledDistance = 0.0f;
};

