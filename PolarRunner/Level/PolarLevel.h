#pragma once

#include <Game/ObstacleType.h>
#include <Level/Level.h>
#include <vector>

class PolarObstacle;
class PolarPlayer;

class PolarLevel : public Craft::Level
{
public:
	virtual void OnInitialized() override;
	virtual void Tick(float deltaTime) override;
	virtual void Draw() override;

	int DistanceToScreenY(float distance) const;
	int GetRoadCenterX(int screenY) const;
	int GetRoadHalfWidth(int screenY) const;
	int GetRoadScreenX(float horizontalPosition, int screenY) const;

	inline int GetPlayerScreenY() const { return playerScreenY; }
	inline float GetRunSpeed() const { return runSpeed; }
	inline float GetViewDistance() const { return viewDistance; }
	inline bool IsPlaying() const { return state == State::Playing; }

private:
	void BuildTestCourse();
	void CheckObstacleCollisions();
	void DrawPerspectiveRoad();
	void DrawHud();
	void UpdateRunSpeed();
	void UpdateCurve(float deltaTime);
	const char* GetObstacleName(ObstacleType type) const;
	const char* GetCurveDirection() const;

	enum class State { Playing, Crashed, Goal };

	int screenWidth = 80;
	int screenHeight = 30;
	int horizonY = 4;
	int playerScreenY = 27;
	float viewDistance = 100.0f;
	float runSpeed = 12.0f;
	float courseDistance = 500.0f;
	float traveledDistance = 0.0f;
	float curveStrength = 0.0f;
	State state = State::Playing;
	ObstacleType crashedObstacleType = ObstacleType::LowSpike;
	std::shared_ptr<PolarPlayer> player;
	std::vector<std::shared_ptr<PolarObstacle>> obstacles;
};
