#pragma once

#include <Actor/Actor.h>
#include <Game/ObstacleType.h>

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
	inline ObstacleType GetObstacleType() const { return obstacleType; }
	inline float GetHorizontalHalfWidth() const { return horizontalHalfWidth; }
	inline bool HasBeenChecked() const { return hasBeenChecked; }
	inline void MarkChecked() { hasBeenChecked = true; }

private:
	float horizontalPosition = 0.0f;
	float distance = 0.0f;
	ObstacleType obstacleType = ObstacleType::LowSpike;
	float horizontalHalfWidth = 0.0f;
	bool hasBeenChecked = false;
};
