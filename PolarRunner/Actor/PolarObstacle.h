#pragma once

#include <Actor/Actor.h>
#include <Game/Lane.h>
#include <Game/ObstacleType.h>

class PolarObstacle : public Craft::Actor
{
	TYPE_DECLARATIONS(PolarObstacle, Craft::Actor)

public:
	PolarObstacle(Lane lane, float distance, ObstacleType type);
	virtual void Tick(float deltaTime) override;
	virtual void Draw() override;

	inline Lane GetLane() const { return lane; }
	inline float GetDistance() const { return distance; }
	inline ObstacleType GetObstacleType() const { return obstacleType; }
	inline bool HasBeenChecked() const { return hasBeenChecked; }
	inline void MarkChecked() { hasBeenChecked = true; }

private:
	Lane lane = Lane::Center;
	float distance = 0.0f;
	ObstacleType obstacleType = ObstacleType::LowSpike;
	bool hasBeenChecked = false;
};
