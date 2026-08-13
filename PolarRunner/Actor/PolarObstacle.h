#pragma once

#include <Actor/Actor.h>
#include <Game/ObstacleType.h>

// IceWall처럼 화면상 실제로 그려지는 폭이 거리에 따라 달라지는 장애물의
// 충돌 판정용 화면 좌표 범위.
struct ScreenBounds
{
	int left = 0;
	int right = 0;
	int top = 0;
	int bottom = 0;
};

class PolarObstacle : public Craft::Actor
{
	TYPE_DECLARATIONS(PolarObstacle, Craft::Actor)

public:
	PolarObstacle(float horizontalPosition, float distance, ObstacleType type,
		float horizontalHalfWidth);
	virtual void Tick(float deltaTime) override;
	virtual void Draw() override;

	inline float GetHorizontalPosition() const { return horizontalPosition; }
	inline float GetDistance() const { return distance; }
	inline float GetPreviousDistance() const { return previousDistance; }
	inline ObstacleType GetObstacleType() const { return obstacleType; }
	inline float GetHorizontalHalfWidth() const { return horizontalHalfWidth; }
	inline bool HasBeenChecked() const { return hasBeenChecked; }
	inline void MarkChecked() { hasBeenChecked = true; }

	// Draw()가 그리는 IceWall 모양 중, 지붕(뾰족한 장식)을 제외한 단단한
	// 몸통("|####|"/"|######|") 부분만의 화면 좌표 범위를 계산합니다.
	ScreenBounds GetIceWallScreenBounds() const;

private:
	float horizontalPosition = 0.0f;
	float distance = 0.0f;
	float previousDistance = 0.0f;
	ObstacleType obstacleType = ObstacleType::LowSpike;
	float horizontalHalfWidth = 0.0f;
	int visualVariant = 0;
	bool hasBeenChecked = false;
};
