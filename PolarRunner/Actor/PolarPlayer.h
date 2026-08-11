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
	inline bool IsJumping() const { return isJumping; }
	bool IsAboveObstacle() const;
	int GetJumpScreenOffset() const;

private:
	float horizontalPosition = 0.0f;
	float horizontalHalfWidth = 0.18f;
	float horizontalVelocity = 0.0f;
	float horizontalSpeed = 1.8f;
	bool isJumping = false;
	float jumpTimer = 0.0f;
	float jumpDuration = 0.85f;
	float jumpInputBufferRemaining = 0.0f;
	float jumpInputBufferDuration = 0.10f;
};
