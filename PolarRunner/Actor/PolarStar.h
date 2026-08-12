#pragma once

#include <Actor/Actor.h>

class PolarStar : public Craft::Actor
{
	TYPE_DECLARATIONS(PolarStar, Craft::Actor)

public:
	PolarStar(float horizontalPosition, float distance);
	virtual void Tick(float deltaTime) override;
	virtual void Draw() override;

	inline float GetHorizontalPosition() const { return horizontalPosition; }
	inline float GetDistance() const { return distance; }
	inline float GetPreviousDistance() const { return previousDistance; }
	inline bool IsCollected() const { return isCollected; }
	void Collect();

private:
	float horizontalPosition = 0.0f;
	float distance = 0.0f;
	float previousDistance = 0.0f;
	bool isCollected = false;
};
