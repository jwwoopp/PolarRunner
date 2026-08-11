#pragma once

#include <Actor/Actor.h>
#include <Game/Lane.h>

class PolarPlayer : public Craft::Actor
{
	TYPE_DECLARATIONS(PolarPlayer, Craft::Actor)

public:
	PolarPlayer();
	virtual void Tick(float deltaTime) override;
	virtual void Draw() override;

	inline Lane GetLane() const { return lane; }
	inline bool IsJumping() const { return isJumping; }
	bool IsAboveObstacle() const;
	int GetJumpScreenOffset() const;
	inline float GetVisualLanePosition() const { return visualLanePosition; }

private:
	Lane lane = Lane::Center;
	float visualLanePosition = 0.0f;
	float laneMoveStart = 0.0f;
	float laneMoveElapsed = 0.0f;
	float laneMoveDuration = 0.10f;
	bool isJumping = false;
	float jumpTimer = 0.0f;
	float jumpDuration = 0.85f;
	float jumpInputBufferRemaining = 0.0f;
	float jumpInputBufferDuration = 0.10f;
};
