#pragma once

#include <Game/ObstacleType.h>
#include <Level/Level.h>
#include <string>
#include <vector>

class PolarObstacle;
class PolarPlayer;

class PolarLevel : public Craft::Level
{
public:
	enum class TerrainType
	{
		Snowfield,
		Coast,
		Canyon,
		NarrowIcePath,
		BrokenIce,
		ResearchBase
	};

	struct RoadSlice
	{
		float distance = 0.0f;
		float centerOffset = 0.0f;
		float width = 1.0f;
		TerrainType terrain = TerrainType::Snowfield;
		int centerX = 0;
		int halfWidth = 2;
	};

	virtual void OnInitialized() override;
	virtual void Tick(float deltaTime) override;
	virtual void Draw() override;

	int DistanceToScreenY(float distance) const;
	int GetRoadCenterX(int screenY) const;
	int GetRoadHalfWidth(int screenY) const;
	int GetRoadScreenX(float horizontalPosition, int screenY) const;
	RoadSlice CalculateRoadSlice(float depth) const;
	float GetPlayerHorizontalMin() const;
	float GetPlayerHorizontalMax() const;

	int GetPlayerScreenY() const;
	inline float GetRunSpeed() const { return runSpeed; }
	inline float GetViewDistance() const { return viewDistance; }
	inline bool IsPlaying() const { return state == State::Playing; }
	bool IsOnNarrowIcePath() const;

private:
	float ScreenYToDistance(int screenY) const;
	float ScreenYToRoadDepth(int screenY) const;
	RoadSlice GetRoadProfile(float coursePosition) const;
	void BuildRoadCourse();
	void BuildTestCourse();
	void CheckObstacleCollisions();
	void CheckTerrainHazards();
	void DrawSkyAndHorizon();
	void DrawPerspectiveRoad();
	void DrawNarrowPathWarningSign();
	void DrawSnowfieldRow(int y, float depth, const RoadSlice& slice);
	void DrawCoastRow(int y, float depth, const RoadSlice& slice);
	void DrawCanyonRow(int y, float depth, const RoadSlice& slice);
	void DrawNarrowIcePathRow(int y, float depth, const RoadSlice& slice);
	void DrawBrokenIceRow(int y, float depth, const RoadSlice& slice);
	void DrawResearchBaseRow(int y, float depth, const RoadSlice& slice);
	void DrawSnowSurface(int y, int startX, int endX);
	void DrawOcean(int y, int startX, int endX);
	void DrawCanyonWall(int y, int startX, int endX);
	void DrawBrokenIce(int y, int startX, int endX);
	void DrawResearchBase(int y, int startX, int endX);
	void DrawHud();
	float GetDisplayDistanceMeters() const;
	void UpdateRunSpeed(float deltaTime);
	void UpdateSpeedNotification(float deltaTime);
	void UpdateCurve(float deltaTime);
	
	// 추가
	void HandleMenuInput();
	void DrawPauseMenu();
	void RetryGame();

	enum class State
	{
		Playing,
		PauseMenu,
		Crashed,
		Goal
	};

	enum class MenuItem
	{
		Resume,
		Retry,
		Quit,
		Length
	};
	
	const char* GetObstacleName(ObstacleType type) const;
	const char* GetCurveDirection() const;


	int screenWidth = 80;
	int screenHeight = 30;
	int horizonY = 7;
	int playerScreenY = 24;
	float viewDistance = 100.0f;
	float runSpeed = 16.0f;
	float courseDistance = 1000.0f;
	float traveledDistance = 0.0f;
	float curveStrength = 0.0f;
	float speedNotificationTimer = 0.0f;
	int speedNotificationStage = 0;
	std::string speedNotification;
	State state = State::Playing;
	State stateBeforePause = State::Playing;
	MenuItem selectedMenuItem = MenuItem::Resume;
	ObstacleType crashedObstacleType = ObstacleType::LowSpike;
	bool fellFromNarrowIcePath = false;
	bool fellThroughBrokenBridge = false;
	std::shared_ptr<PolarPlayer> player;
	std::vector<RoadSlice> roadSlices;
	std::vector<std::shared_ptr<PolarObstacle>> obstacles;
};
