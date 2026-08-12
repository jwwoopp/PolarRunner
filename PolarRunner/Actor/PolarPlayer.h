#pragma once

#include <Actor/Actor.h>

class PolarPlayer : public Craft::Actor
{
	TYPE_DECLARATIONS(PolarPlayer, Craft::Actor)

public:
	PolarPlayer();
	virtual void Tick(float deltaTime) override;
	virtual void Draw() override;

	inline float GetHorizontalPosition() const { return horizontalPosition; }
	inline float GetHorizontalHalfWidth() const { return horizontalHalfWidth; }
	inline int GetLongitudinalScreenOffset() const
	{
		return static_cast<int>(longitudinalScreenOffset);
	}
	inline bool IsJumping() const { return isJumping; }
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
	float longitudinalScreenOffset = 0.0f;
	float longitudinalSpeed = 7.0f;
	bool isJumping = false;
	float jumpTimer = 0.0f;
	float jumpDuration = 1.0f;
	float jumpInputBufferRemaining = 0.0f;
	float jumpInputBufferDuration = 0.10f;
};
