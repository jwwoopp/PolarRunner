#pragma once

#include <Actor/Actor.h>

class Player : public Craft::Actor
{
	TYPE_DECLARATIONS(Player, Craft::Actor)

public:
	Player();
	virtual void Tick(float deltaTime) override;
	virtual void Draw() override;

	inline float GetHorizontalPosition() const { return horizontalPosition; }
	inline float GetHorizontalHalfWidth() const { return horizontalHalfWidth; }
	inline int GetLongitudinalScreenOffset() const
	{
		return static_cast<int>(longitudinalScreenOffset);
	}
	inline bool IsJumping() const { return isJumping; }
	inline bool IsFacingRight() const { return isFacingRight; }
	// Draw()가 "(_____)" 몸통을 그리는 시작 X(중심 x 기준 오프셋). 충돌
	// 판정도 같은 값을 써야 그려지는 위치와 어긋나지 않습니다.
	inline int GetBodyLeftOffset() const { return isFacingRight ? -5 : -1; }
	// "(_____)"는 7칸이라 왼쪽 끝에서 오른쪽 끝까지 6칸 떨어져 있습니다.
	static constexpr int BodyWidth = 7;
	bool IsAboveObstacle() const;
	float GetJumpHeight() const;
	int GetJumpScreenOffset() const;

private:
	float horizontalPosition = 0.0f;
	// Covers the sliding penguin's central body, including the right-side ')'.
	// The long ASCII trail on the left remains visual-only for fair near misses.
	float horizontalHalfWidth = 0.13f;
	float horizontalVelocity = 0.0f;
	float horizontalSpeed = 1.8f;
	bool isFacingRight = false;
	float longitudinalScreenOffset = 0.0f;
	float longitudinalSpeed = 7.0f;
	bool isJumping = false;
	float jumpTimer = 0.0f;
	float jumpDuration = 1.0f;
	float jumpInputBufferRemaining = 0.0f;
	float jumpInputBufferDuration = 0.10f;
};
