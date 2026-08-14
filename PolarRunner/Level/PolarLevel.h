#pragma once

#include <Game/ObstacleType.h>
#include <Level/Level.h>
#include <Level/MenuLevel.h>
#include <Actor/EnemyBullet.h>
#include <functional>
#include <string>
#include <vector>

class PolarObstacle;
class Player;
class PolarStar;
class Enemy;

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
	// 다음 PolarLevel이 시작할 코스 위치입니다. 타이틀의 테스트 진입에서만
	// 사용하며, 일반 시작은 0m를 전달합니다.
	static void SetNextStartDistance(float distance);

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
	void CheckEnemyBulletCollisions();
	void CheckStarCollections();
	void HandlePlayerFire();
	void UpdateCoastEnemy(float deltaTime);
	bool IsNarrowIcePathAhead(float lookAheadDistance) const;
	void CheckTerrainHazards(float deltaTime);
	void DrawSkyAndHorizon();
	void DrawPerspectiveRoad();
	void DrawNarrowPathWarningSign();
	void DrawSnowfieldRow(int y, float depth, const RoadSlice& slice);
	void DrawCoastRow(int y, float depth, const RoadSlice& slice);
	void DrawCanyonRow(int y, float depth, const RoadSlice& slice);
	void DrawNarrowIcePathRow(int y, float depth, const RoadSlice& slice);
	void DrawBrokenIceRow(int y, float depth, const RoadSlice& slice);
	void DrawResearchBaseRow(int y, float depth, const RoadSlice& slice);
	// leftFill/rightFill로 도로 좌/우 배경을 채우고, depth 기준으로 가장자리
	// 글자를 고르는 6개 Draw*Row 함수의 공용 골격.
	void DrawRoadEdges(int y, const RoadSlice& slice,
		void (PolarLevel::*leftFill)(int, int, int),
		void (PolarLevel::*rightFill)(int, int, int),
		float depth, float edgeDepthThreshold,
		const char* leftGlyphNear, const char* leftGlyphFar,
		const char* rightGlyphNear, const char* rightGlyphFar,
		Craft::Color leftColor, Craft::Color rightColor,
		int sortingOrder = 0);
	void DrawSnowSurface(int y, int startX, int endX);
	void DrawOcean(int y, int startX, int endX);
	void DrawCanyonWall(int y, int startX, int endX);
	void DrawBrokenIce(int y, int startX, int endX);
	void DrawResearchBase(int y, int startX, int endX);
	// 배경 채우기 함수들이 공유하는 "칸마다 조건 검사 후 glyph 선택" 골격.
	void DrawTerrainFill(int y, int startX, int endX, Craft::Color color,
		const std::function<bool(int x, int y, float depth)>& shouldDraw,
		const std::function<const char*(int x, int y)>& glyphFor);
	void DrawHud();
	float GetDisplayDistanceMeters() const;
	void UpdateRunSpeed(float deltaTime);
	void UpdateSpeedNotification(float deltaTime);
	void UpdateCurve(float deltaTime);
	
	// 추가
	void HandleMenuInput();
	void OpenMenu(bool canResume, MenuLevel::Item defaultItem);
	void RetryGame();

	// 플레이테스트 계측
	void LogEvent(const std::string& message) const;
	void LogObstacleLayout() const;

	enum class State
	{
		Playing,
		Crashed,
		Goal
	};

	const char* GetObstacleName(ObstacleType type) const;
	const char* GetCurveDirection() const;


	int screenWidth = 80;
	int screenHeight = 30;
	int horizonY = 7;
	int playerScreenY = 24;
	float viewDistance = 100.0f;
	float runSpeed = 16.0f;
	float courseDistance = 15000.0f;
	float traveledDistance = 0.0f;
	inline static float nextStartDistance = 0.0f;
	float curveStrength = 0.0f;
	float speedNotificationTimer = 0.0f;
	int speedNotificationStage = 0;
	int collectedStarCount = 0;
	int nonShotCount = 0;
	static constexpr int RequiredStarCount = 5;
	std::string speedNotification;
	State state = State::Playing;
	ObstacleType crashedObstacleType = ObstacleType::LowSpike;
	bool fellFromNarrowIcePath = false;
	bool fellThroughBrokenBridge = false;
	bool hitByEnemyBullet = false;
	float narrowPathFallTimer = 0.0f;
	std::shared_ptr<Player> player;
	std::vector<RoadSlice> roadSlices;
	std::vector<std::shared_ptr<PolarObstacle>> obstacles;
	std::vector<std::shared_ptr<PolarStar>> stars;
	std::vector<std::shared_ptr<EnemyBullet>> enemyBullets;
	std::shared_ptr<Enemy> coastEnemy;
	int nextCoastEnemyZoneIndex = 0;
	float coastEnemyWarningTimer = 0.0f;
	// Enemy 구간(zone)별로 바다가 왼쪽/오른쪽 중 어디에 그려지는지.
	// BuildTestCourse에서 채워지고 DrawCoastRow/UpdateCoastEnemy가 함께 참조.
	std::vector<bool> coastOceanOnRight;
	float enemyFireTimer = 0.0f;
	float coastEnemyScreenX = 0.0f;
	int enemyFirePatternIndex = 0;
	bool coastEnemyWasActive = false;
	float coastVisualHoldTimer = 0.0f;
	bool heldCoastOceanOnRight = false;
	float nextBalanceLogDistance = 100.0f;
};
