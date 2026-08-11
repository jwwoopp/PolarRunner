#include "PolarLevel.h"

#include <Actor/PolarObstacle.h>
#include <Actor/PolarPlayer.h>
#include <Engine/Engine.h>
#include <Input/Input.h>
#include <Math/Color.h>
#include <Render/Renderer.h>
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <random>
#include <sstream>

void PolarLevel::OnInitialized()
{
	Level::OnInitialized();
	screenWidth = Craft::Engine::Get().GetWidth();
	screenHeight = Craft::Engine::Get().GetHeight();
	horizonY = 7;
	playerScreenY = screenHeight - 6;
	player = SpawnActor<PolarPlayer>();
	BuildRoadCourse();
	BuildTestCourse();
}

void PolarLevel::BuildRoadCourse()
{
	roadSlices =
	{
		// 넓은 시작 설원
		{ 0.0f,    0.00f, 1.25f, TerrainType::Snowfield },
		{ 40.0f,   0.00f, 1.25f, TerrainType::Snowfield },

		// 왼쪽 해안
		{ 80.0f,  -0.55f, 0.90f, TerrainType::Coast },
		{ 150.0f, -0.55f, 0.82f, TerrainType::Coast },

		// 오른쪽으로 꺾이는 협곡
		{ 180.0f,  0.10f, 0.65f, TerrainType::Canyon },
		{ 230.0f,  0.60f, 0.55f, TerrainType::Canyon },

		// 반대 방향 깨진 빙판
		{ 260.0f,  0.00f, 0.95f, TerrainType::BrokenIce },
		{ 300.0f, -0.60f, 0.88f, TerrainType::BrokenIce },
		{ 360.0f, -0.60f, 0.92f, TerrainType::BrokenIce },

		// 연구기지 S자 구간
		{ 400.0f,  0.00f, 1.05f, TerrainType::ResearchBase },
		{ 440.0f,  0.60f, 0.92f, TerrainType::ResearchBase },
		{ 490.0f, -0.60f, 0.88f, TerrainType::ResearchBase },
		{ 540.0f,  0.55f, 0.92f, TerrainType::ResearchBase },

		// 넓은 설원
		{ 580.0f,  0.00f, 1.30f, TerrainType::Snowfield },
		{ 650.0f, -0.45f, 1.15f, TerrainType::Snowfield },
		{ 710.0f,  0.45f, 1.10f, TerrainType::Snowfield },

		// 좁은 얼음다리
		{ 750.0f,  0.00f, 0.55f, TerrainType::NarrowIcePath },
		{ 810.0f, -0.25f, 0.40f, TerrainType::NarrowIcePath },
		{ 870.0f,  0.25f, 0.40f, TerrainType::NarrowIcePath },

		// 북극 도착 구간
		{ 900.0f,  0.00f, 1.10f, TerrainType::Snowfield },
		{ 950.0f, -0.35f, 1.20f, TerrainType::Snowfield },
		{ 1000.0f, 0.00f, 1.35f, TerrainType::Snowfield }
	};
}

void PolarLevel::BuildTestCourse()
{
	// Widths use the same normalized road coordinates as player movement and
	// match the near-camera four-column spike and eight-column wall sprites.
	constexpr float spikeHalfWidth = 0.055f;
	constexpr float wallHalfWidth = 0.11f;
	const auto add = [this](float horizontalPosition, float distance,
		ObstacleType type, float horizontalHalfWidth)
	{
		obstacles.emplace_back(SpawnActor<PolarObstacle>(
			horizontalPosition, distance, type, horizontalHalfWidth));
	};
	const auto spike = [&add](float x, float distance)
	{
		add(x, distance, ObstacleType::LowSpike, spikeHalfWidth);
	};
	const auto wall = [&add](float x, float distance)
	{
		add(x, distance, ObstacleType::IceWall, wallHalfWidth);
	};
	const auto brokenBridge = [&add](float distance)
	{
		add(0.0f, distance, ObstacleType::BrokenBridge, 1.0f);
	};

	// Keep the first obstacle fixed so every run teaches jumping before random
	// decisions begin.
	spike(0.0f, 55.0f);

	std::random_device seed;
	std::mt19937 random(seed());
	const float lanes[] = { -0.55f, 0.0f, 0.55f };
	std::uniform_int_distribution<int> laneChoice(0, 2);
	std::uniform_int_distribution<int> typeChoice(0, 1);
	std::uniform_real_distribution<float> spacingChoice(48.0f, 72.0f);

	// One obstacle per distance guarantees at least two lateral escape routes.
	// Spacing is also randomized, so Retry produces a different rhythm.
	for (float distance = 125.0f; distance < 730.0f;
		distance += spacingChoice(random))
	{
		const float lane = lanes[laneChoice(random)];
		if (typeChoice(random) == 0)
		{
			spike(lane, distance);
		}
		else
		{
			wall(lane, distance);
		}
	}

	// Broken bridges stay inside the narrow-path section, but their exact
	// positions vary while retaining enough recovery distance between jumps.
	std::uniform_real_distribution<float> bridgeJitter(-8.0f, 8.0f);
	brokenBridge(790.0f + bridgeJitter(random));
	brokenBridge(850.0f + bridgeJitter(random));

	std::uniform_real_distribution<float> finalLane(-0.55f, 0.55f);
	spike(finalLane(random), 915.0f);
	wall(finalLane(random), 965.0f);
}

void PolarLevel::Tick(float deltaTime)
{
	HandleMenuInput();
	if (!IsPlaying())
	{
		return;
	}

	UpdateCurve(deltaTime);
	UpdateRunSpeed(deltaTime);
	Level::Tick(deltaTime);
	traveledDistance += runSpeed * deltaTime;
	CheckTerrainHazards();
	CheckObstacleCollisions();
	if (traveledDistance >= courseDistance && IsPlaying())
	{
		state = State::Goal;
	}
}

void PolarLevel::CheckObstacleCollisions()
{
	if (!player)
	{
		return;
	}
	for (const std::shared_ptr<PolarObstacle>& obstacle : obstacles)
	{
		if (obstacle->HasBeenChecked())
		{
			continue;
		}

		const int obstacleScreenY = DistanceToScreenY(obstacle->GetDistance());
		const int currentPlayerScreenY = GetPlayerScreenY();
		const bool isBrokenBridge =
			obstacle->GetObstacleType() == ObstacleType::BrokenBridge;
		const int contactScreenY = isBrokenBridge
			? obstacleScreenY - 1
			: obstacleScreenY;
		// Once an obstacle has visibly passed the player's current row it must
		// not become dangerous again if the player moves backward.
		if (contactScreenY > currentPlayerScreenY + 1
			|| obstacle->GetDistance() < -2.0f)
		{
			obstacle->MarkChecked();
			continue;
		}

		// Compare positions in the shared road coordinate system. Comparing two
		// projected screen X values made nearby rows diverge on curved roads.
		const float combinedHalfWidth = player->GetHorizontalHalfWidth()
			+ obstacle->GetHorizontalHalfWidth();
		const bool horizontalOverlap =
			std::abs(player->GetHorizontalPosition()
				- obstacle->GetHorizontalPosition()) < combinedHalfWidth;
		// Collision follows the player's movable screen row. A one-row window
		// prevents a fast obstacle from slipping through between rendered rows.
		const bool reachesPlayer = isBrokenBridge
			? contactScreenY >= currentPlayerScreenY
				&& contactScreenY <= currentPlayerScreenY + 1
			: std::abs(contactScreenY - currentPlayerScreenY) <= 1;
		const bool clearedLowSpike =
			obstacle->GetObstacleType() == ObstacleType::LowSpike
			&& player->IsAboveObstacle();
		// The upright airborne pose must agree with the gameplay result. Bridge
		// gaps require a jump state, while low spikes still require real height.
		const bool jumpIntent = player->IsJumping()
			|| Craft::Input::Get().GetKey(VK_SPACE)
			|| Craft::Input::Get().GetKeyDown(VK_SPACE);
		const bool clearedBrokenBridge = isBrokenBridge && jumpIntent;
		if ((isBrokenBridge || horizontalOverlap) && reachesPlayer
			&& !clearedLowSpike && !clearedBrokenBridge)
		{
			crashedObstacleType = obstacle->GetObstacleType();
			fellThroughBrokenBridge = isBrokenBridge;
			state = State::Crashed;
			return;
		}
	}
}

void PolarLevel::CheckTerrainHazards()
{
	if (!player || GetRoadProfile(traveledDistance).terrain
		!= TerrainType::NarrowIcePath)
	{
		return;
	}
	// Leave a small warning margin near the rail instead of killing the player
	// while the penguin still appears comfortably inside the bridge.
	if (std::abs(player->GetHorizontalPosition()) > 0.98f)
	{
		fellFromNarrowIcePath = true;
		state = State::Crashed;
	}
}

int PolarLevel::GetPlayerScreenY() const
{
	const int offset = player ? player->GetLongitudinalScreenOffset() : 0;
	return std::clamp(playerScreenY + offset, horizonY + 3, screenHeight - 2);
}

float PolarLevel::GetPlayerHorizontalMin() const
{
	const TerrainType terrain = GetRoadProfile(traveledDistance).terrain;
	if (terrain == TerrainType::Coast)
	{
		return -0.62f;
	}
	return terrain == TerrainType::NarrowIcePath ? -1.15f : -1.0f;
}

float PolarLevel::GetPlayerHorizontalMax() const
{
	const TerrainType terrain = GetRoadProfile(traveledDistance).terrain;
	return terrain == TerrainType::NarrowIcePath ? 1.15f : 1.0f;
}

const char* PolarLevel::GetObstacleName(ObstacleType type) const
{
	if (type == ObstacleType::IceWall) return "ICE WALL";
	if (type == ObstacleType::BrokenBridge) return "BROKEN BRIDGE";
	return "LOW SPIKE";
}

int PolarLevel::DistanceToScreenY(float distance) const
{
	const float normalized = std::clamp(1.0f - distance / viewDistance, 0.0f, 1.0f);
	const float perspective = std::pow(normalized, 1.55f);
	const int roadTopY = horizonY + 1;
	const int roadBottomY = screenHeight - 1;
	return roadTopY + static_cast<int>(perspective * (roadBottomY - roadTopY));
}

float PolarLevel::ScreenYToDistance(int screenY) const
{
	const float perspective = ScreenYToRoadDepth(screenY);
	const float normalized = std::pow(perspective, 1.0f / 1.55f);
	return (1.0f - normalized) * viewDistance;
}

float PolarLevel::ScreenYToRoadDepth(int screenY) const
{
	const int roadBottomY = screenHeight - 1;
	const int roadTopY = horizonY + 1;
	return std::clamp(static_cast<float>(screenY - roadTopY)
		/ (roadBottomY - roadTopY), 0.0f, 1.0f);
}

PolarLevel::RoadSlice PolarLevel::GetRoadProfile(float coursePosition) const
{
	if (roadSlices.empty() || coursePosition <= roadSlices.front().distance)
	{
		return roadSlices.empty() ? RoadSlice{} : roadSlices.front();
	}
	for (size_t index = 1; index < roadSlices.size(); ++index)
	{
		const RoadSlice& next = roadSlices[index];
		if (coursePosition <= next.distance)
		{
			const RoadSlice& previous = roadSlices[index - 1];
			float amount = (coursePosition - previous.distance)
				/ (next.distance - previous.distance);
			amount = amount * amount * (3.0f - 2.0f * amount);
			return { coursePosition,
				previous.centerOffset
					+ (next.centerOffset - previous.centerOffset) * amount,
				previous.width + (next.width - previous.width) * amount,
				amount < 0.5f ? previous.terrain : next.terrain };
		}
	}
	return roadSlices.back();
}

PolarLevel::RoadSlice PolarLevel::CalculateRoadSlice(float depth) const
{
	depth = std::clamp(depth, 0.0f, 1.0f);
	const float normalizedDistance = std::pow(depth, 1.0f / 1.55f);
	const float forwardDistance = (1.0f - normalizedDistance) * viewDistance;
	RoadSlice slice = GetRoadProfile(traveledDistance + forwardDistance);

	// Suppress distant lateral motion so a curve grows into view instead of
	// folding the narrow horizon. Smoothstep avoids a visible transition row.
	const float curvePerspective = depth * depth * (3.0f - 2.0f * depth);
	const float maximumCurveOffset = screenWidth * 0.18f;
	slice.centerX = screenWidth / 2 + static_cast<int>(
		slice.centerOffset * curvePerspective * maximumCurveOffset);

	// Keep the opening snowfield broad; narrow terrain still applies its own
	// width scale below, so canyon and bridge sections remain distinct.
	const int nearHalfWidth = static_cast<int>(screenWidth * 0.38f);
	const int perspectiveHalfWidth = 2
		+ static_cast<int>(depth * (nearHalfWidth - 2));
	const int maximumVisibleHalfWidth = (std::max)(2,
		(std::min)(slice.centerX - 1, screenWidth - slice.centerX - 2));
	const float widthBlend = depth * depth * (3.0f - 2.0f * depth);
	const float visibleWidth = 1.0f + (slice.width - 1.0f) * widthBlend;
	float minimumWidthScale = 0.72f;
	if (slice.terrain == TerrainType::Canyon)
	{
		minimumWidthScale = 0.45f;
	}
	else if (slice.terrain == TerrainType::NarrowIcePath)
	{
		minimumWidthScale = 0.28f;
	}
	const int minimumHalfWidth = 2 + static_cast<int>(depth
		* (nearHalfWidth * minimumWidthScale - 2.0f));
	slice.halfWidth = std::clamp((std::max)(minimumHalfWidth,
		static_cast<int>(perspectiveHalfWidth * visibleWidth)),
		2, maximumVisibleHalfWidth);
	return slice;
}

int PolarLevel::GetRoadCenterX(int screenY) const
{
	return CalculateRoadSlice(ScreenYToRoadDepth(screenY)).centerX;
}

int PolarLevel::GetRoadHalfWidth(int screenY) const
{
	return CalculateRoadSlice(ScreenYToRoadDepth(screenY)).halfWidth;
}

int PolarLevel::GetRoadScreenX(float horizontalPosition, int screenY) const
{
	const RoadSlice slice = CalculateRoadSlice(ScreenYToRoadDepth(screenY));
	const int usableHalfWidth = (std::max)(1,
		static_cast<int>(slice.halfWidth * 0.72f));
	return slice.centerX
		+ static_cast<int>(horizontalPosition * usableHalfWidth);
}

void PolarLevel::UpdateCurve(float deltaTime)
{
	const float targetStrength = GetRoadProfile(traveledDistance).centerOffset;
	const float blend = (std::min)(deltaTime * 2.8f, 1.0f);
	curveStrength += (targetStrength - curveStrength) * blend;
	if (std::abs(curveStrength) < 0.01f && targetStrength == 0.0f)
	{
		curveStrength = 0.0f;
	}
}

void PolarLevel::HandleMenuInput()
{
	const Craft::Input& input = Craft::Input::Get();
	if (state == State::StartMenu)
	{
		if (input.GetKeyDown(VK_RETURN))
		{
			state = State::Playing;
		}
		else if (input.GetKeyDown(VK_ESCAPE))
		{
			Craft::Engine::Get().Quit();
		}
		return;
	}

	if (state == State::Playing)
	{
		if (input.GetKeyDown(VK_ESCAPE))
		{
			selectedMenuItem = MenuItem::Resume;
			state = State::PauseMenu;
		}
		return;
	}

	if (state == State::PauseMenu)
	{
		if (input.GetKeyDown(VK_ESCAPE))
		{
			state = State::Playing;
			return;
		}

		const int itemCount = static_cast<int>(MenuItem::Length);
		int selectedIndex = static_cast<int>(selectedMenuItem);
		if (input.GetKeyDown(VK_UP))
		{
			selectedIndex = (selectedIndex + itemCount - 1) % itemCount;
		}
		else if (input.GetKeyDown(VK_DOWN))
		{
			selectedIndex = (selectedIndex + 1) % itemCount;
		}
		selectedMenuItem = static_cast<MenuItem>(selectedIndex);

		if (input.GetKeyDown(VK_RETURN))
		{
			switch (selectedMenuItem)
			{
			case MenuItem::Resume:
				state = State::Playing;
				break;
			case MenuItem::Retry:
				RetryGame();
				break;
			case MenuItem::Quit:
				Craft::Engine::Get().Quit();
				break;
			default:
				break;
			}
		}
		return;
	}

	if ((state == State::Crashed || state == State::Goal)
		&& input.GetKeyDown('R'))
	{
		RetryGame();
	}
}

void PolarLevel::RetryGame()
{
	Craft::Engine::Get().AddNewLevel<PolarLevel>();
}

const char* PolarLevel::GetCurveDirection() const
{
	if (curveStrength < -0.08f) return "LEFT";
	if (curveStrength > 0.08f) return "RIGHT";
	return "STRAIGHT";
}

void PolarLevel::UpdateRunSpeed(float deltaTime)
{
	const float baseSpeed = std::abs(curveStrength) > 0.08f ? 14.0f : 16.0f;
	const float courseProgress = std::clamp(
		traveledDistance / courseDistance, 0.0f, 1.0f);
	const float distanceSpeedBonus = courseProgress * 8.0f;
	const float forwardAmount = player ? std::clamp(
		-static_cast<float>(player->GetLongitudinalScreenOffset()) / 8.0f,
		0.0f, 1.0f) : 0.0f;
	const float targetSpeed = baseSpeed + distanceSpeedBonus
		+ forwardAmount * 3.0f;
	const float speedBlend = (std::min)(deltaTime * 2.5f, 1.0f);
	runSpeed += (targetSpeed - runSpeed) * speedBlend;
}

void PolarLevel::DrawSkyAndHorizon()
{
	// The sky and horizon stay screen-fixed; only the road projection curves.
	Craft::Renderer::Get().Submit("*", Craft::Vector2(screenWidth / 5, 3),
		Craft::Color::BrightWhite, 0);
	Craft::Renderer::Get().Submit("*", Craft::Vector2(screenWidth * 3 / 4, 4),
		Craft::Color::BrightWhite, 0);
	Craft::Renderer::Get().Submit(".", Craft::Vector2(screenWidth / 2, 3),
		Craft::Color::BrightWhite, 0);

	const std::string mountains = "_/\\_      _/\\_        _/\\_";
	Craft::Renderer::Get().Submit(mountains,
		Craft::Vector2(screenWidth / 2 - static_cast<int>(mountains.size()) / 2,
			horizonY - 2), Craft::Color::Cyan, 0);
	Craft::Renderer::Get().Submit("/____\\____/____\\______/____\\",
		Craft::Vector2(screenWidth / 2 - 15, horizonY - 1),
		Craft::Color::BrightWhite, 0);
	Craft::Renderer::Get().Submit(std::string(screenWidth, '_'),
		Craft::Vector2(0, horizonY), Craft::Color::BrightWhite, 0);
}

void PolarLevel::DrawPerspectiveRoad()
{
	const int roadTopY = horizonY + 1;
	const int roadBottomY = screenHeight - 1;

	for (int y = roadTopY; y <= roadBottomY; ++y)
	{
		const float depth = ScreenYToRoadDepth(y);
		const RoadSlice slice = CalculateRoadSlice(depth);

		switch (slice.terrain)
		{
		case TerrainType::Snowfield:
			DrawSnowfieldRow(y, depth, slice);
			break;

		case TerrainType::Coast:
			DrawCoastRow(y, depth, slice);
			break;

		case TerrainType::Canyon:
			DrawCanyonRow(y, depth, slice);
			break;

		case TerrainType::NarrowIcePath:
			DrawNarrowIcePathRow(y, depth, slice);
			break;

		case TerrainType::BrokenIce:
			DrawBrokenIceRow(y, depth, slice);
			break;

		case TerrainType::ResearchBase:
			DrawResearchBaseRow(y, depth, slice);
			break;
		}
	}
}

void PolarLevel::DrawSnowfieldRow(int y, float depth, const RoadSlice& slice)
{
	const int leftX = slice.centerX - slice.halfWidth;
	const int rightX = slice.centerX + slice.halfWidth;
	DrawSnowSurface(y, 0, leftX - 1);
	DrawSnowSurface(y, rightX + 1, screenWidth - 1);
	Craft::Renderer::Get().Submit(depth > 0.55f ? "/" : "|",
		Craft::Vector2(leftX, y), Craft::Color::BrightWhite, 0);
	Craft::Renderer::Get().Submit(depth > 0.55f ? "\\" : "|",
		Craft::Vector2(rightX, y), Craft::Color::BrightWhite, 0);
}

void PolarLevel::DrawCoastRow(int y, float depth, const RoadSlice& slice)
{
	const int leftX = slice.centerX - slice.halfWidth;
	const int rightX = slice.centerX + slice.halfWidth;
	DrawOcean(y, 0, leftX - 1);
	DrawSnowSurface(y, rightX + 1, screenWidth - 1);
	Craft::Renderer::Get().Submit(depth > 0.45f ? "~" : ":",
		Craft::Vector2(leftX, y), Craft::Color::Blue, 0);
	Craft::Renderer::Get().Submit("\\", Craft::Vector2(rightX, y),
		Craft::Color::BrightWhite, 0);
}

void PolarLevel::DrawCanyonRow(int y, float depth, const RoadSlice& slice)
{
	const int leftX = slice.centerX - slice.halfWidth;
	const int rightX = slice.centerX + slice.halfWidth;
	DrawCanyonWall(y, 0, leftX - 1);
	DrawCanyonWall(y, rightX + 1, screenWidth - 1);
	const char* wallEdge = depth > 0.5f ? "#" : "|";
	Craft::Renderer::Get().Submit(wallEdge, Craft::Vector2(leftX, y),
		Craft::Color::Cyan, 0);
	Craft::Renderer::Get().Submit(wallEdge, Craft::Vector2(rightX, y),
		Craft::Color::Cyan, 0);
}

void PolarLevel::DrawNarrowIcePathRow(int y, float depth, const RoadSlice& slice)
{
	const int leftX = slice.centerX - slice.halfWidth;
	const int rightX = slice.centerX + slice.halfWidth;
	DrawOcean(y, 0, leftX - 1);
	DrawOcean(y, rightX + 1, screenWidth - 1);

	// Perspective rails make the strip read as a bridge rather than a tunnel.
	Craft::Renderer::Get().Submit("/", Craft::Vector2(leftX, y),
		Craft::Color::BrightWhite, 1);
	Craft::Renderer::Get().Submit("\\", Craft::Vector2(rightX, y),
		Craft::Color::BrightWhite, 1);

	// Keep the intact ice quiet so the dense blue gap is immediately readable.
	for (int x = leftX + 1; x < rightX; ++x)
	{
		if ((x + y) % (depth > 0.55f ? 4 : 6) == 0)
		{
			const char* deckGlyph = (x + y) % 3 == 0 ? "/" : "_";
			Craft::Renderer::Get().Submit(deckGlyph, Craft::Vector2(x, y),
				Craft::Color::BrightWhite, 0);
		}
	}
}

void PolarLevel::DrawBrokenIceRow(int y, float depth, const RoadSlice& slice)
{
	const int leftX = slice.centerX - slice.halfWidth;
	const int rightX = slice.centerX + slice.halfWidth;
	DrawOcean(y, 0, leftX - 1);
	DrawOcean(y, rightX + 1, screenWidth - 1);
	DrawBrokenIce(y, leftX + 1, rightX - 1);
	Craft::Renderer::Get().Submit(depth > 0.5f ? "/" : "|",
		Craft::Vector2(leftX, y), Craft::Color::Cyan, 0);
	Craft::Renderer::Get().Submit(depth > 0.5f ? "\\" : "|",
		Craft::Vector2(rightX, y), Craft::Color::Cyan, 0);
}

void PolarLevel::DrawResearchBaseRow(int y, float depth, const RoadSlice& slice)
{
	const int leftX = slice.centerX - slice.halfWidth;
	const int rightX = slice.centerX + slice.halfWidth;
	DrawResearchBase(y, 0, leftX - 1);
	DrawResearchBase(y, rightX + 1, screenWidth - 1);
	const char* leftEdge = depth > 0.45f ? "[" : "|";
	const char* rightEdge = depth > 0.45f ? "]" : "|";
	Craft::Renderer::Get().Submit(leftEdge, Craft::Vector2(leftX, y),
		Craft::Color::BrightWhite, 0);
	Craft::Renderer::Get().Submit(rightEdge, Craft::Vector2(rightX, y),
		Craft::Color::BrightWhite, 0);
}

void PolarLevel::DrawCanyonWall(int y, int startX, int endX)
{
	startX = std::clamp(startX, 0, screenWidth - 1);
	endX = std::clamp(endX, 0, screenWidth - 1);
	for (int x = startX; x <= endX; ++x)
	{
		if ((x + y) % 2 == 0)
		{
			const char* glyph = (x + y) % 6 == 0 ? "#" : ".";
			Craft::Renderer::Get().Submit(glyph, Craft::Vector2(x, y),
				Craft::Color::Cyan, 0);
		}
	}
}

void PolarLevel::DrawSnowSurface(int y, int startX, int endX)
{
	startX = std::clamp(startX, 0, screenWidth - 1);
	endX = std::clamp(endX, 0, screenWidth - 1);
	for (int x = startX; x <= endX; ++x)
	{
		if ((x + y * 2) % 5 == 0)
		{
			const char* glyph = (x + y) % 10 == 0 ? "*" : ".";
			Craft::Renderer::Get().Submit(glyph, Craft::Vector2(x, y),
				Craft::Color::BrightWhite, 0);
		}
	}
}

void PolarLevel::DrawOcean(int y, int startX, int endX)
{
	startX = std::clamp(startX, 0, screenWidth - 1);
	endX = std::clamp(endX, 0, screenWidth - 1);
	for (int x = startX; x <= endX; ++x)
	{
		if ((x + y) % 3 == 0)
		{
			const char* glyph = (x + y) % 6 == 0 ? "~" : "-";
			Craft::Renderer::Get().Submit(glyph, Craft::Vector2(x, y),
				Craft::Color::Blue, 0);
		}
	}
}

void PolarLevel::DrawBrokenIce(int y, int startX, int endX)
{
	startX = std::clamp(startX, 0, screenWidth - 1);
	endX = std::clamp(endX, 0, screenWidth - 1);
	const float depth = ScreenYToRoadDepth(y);
	const int spacing = 17 - static_cast<int>(depth * 9.0f);
	for (int x = startX; x <= endX; ++x)
	{
		if ((x * 3 + y * 5) % spacing == 0)
		{
			const char* glyph = (x + y) % 2 == 0 ? "/" : "\\";
			Craft::Renderer::Get().Submit(glyph, Craft::Vector2(x, y),
				Craft::Color::Cyan, 0);
		}
	}
}

void PolarLevel::DrawResearchBase(int y, int startX, int endX)
{
	startX = std::clamp(startX, 0, screenWidth - 1);
	endX = std::clamp(endX, 0, screenWidth - 1);
	const float depth = ScreenYToRoadDepth(y);
	const int spacing = 10 - static_cast<int>(depth * 5.0f);
	for (int x = startX; x <= endX; ++x)
	{
		if ((x + y * 2) % spacing == 0)
		{
			const char* glyph = (y % 4 == 0) ? "#" : ((x % 5 == 0) ? "|" : "_");
			Craft::Renderer::Get().Submit(glyph, Craft::Vector2(x, y),
				Craft::Color::BrightWhite, 0);
		}
	}
}

void PolarLevel::DrawHud()
{
	const float displayDistance = GetDisplayDistanceMeters();
	std::ostringstream hud;
	hud << "POLAR RUNNER  DIST: ";
	if (displayDistance < 1000.0f)
	{
		hud << static_cast<int>(displayDistance) << " m";
	}
	else
	{
		hud << std::fixed << std::setprecision(1)
			<< displayDistance / 1000.0f << " km";
	}
	hud << std::fixed << std::setprecision(1)
		<< "  SPEED: " << runSpeed
		<< "  ROAD: " << GetCurveDirection();
	Craft::Renderer::Get().Submit(hud.str(), Craft::Vector2(1, 0),
		Craft::Color::BrightWhite, 1000);
	Craft::Renderer::Get().Submit("ARROWS: Glide / Forward / Back   SPACE: Jump   ESC: Menu",
		Craft::Vector2(1, 1), Craft::Color::Cyan, 1000);

	if (state == State::Crashed)
	{
		const std::string message = fellThroughBrokenBridge
			? "CRASH: FELL THROUGH BROKEN BRIDGE   R : Retry"
			: (fellFromNarrowIcePath
				? "CRASH: FELL OFF NARROW ICE PATH   R : Retry"
				: std::string("CRASH: ") + GetObstacleName(crashedObstacleType)
					+ "   R : Retry");
		const int messageY = fellThroughBrokenBridge
			? horizonY + 2 : screenHeight / 2;
		Craft::Renderer::Get().Submit(message,
			Craft::Vector2(screenWidth / 2 - static_cast<int>(message.size()) / 2,
				messageY),
			Craft::Color::Red, 2000);
	}
	else if (state == State::Goal)
	{
		Craft::Renderer::Get().Submit("GOAL! TEST COURSE COMPLETE   R : Retry",
			Craft::Vector2(screenWidth / 2 - 18, screenHeight / 2),
			Craft::Color::Green, 2000);
	}
}

float PolarLevel::GetDisplayDistanceMeters() const
{
	const float courseProgress = std::clamp(
		traveledDistance / courseDistance, 0.0f, 1.0f);
	constexpr float finalDistanceKm = 20004.0f;
	constexpr float meterPhaseEnd = 0.05f;

	float displayDistanceKm = 0.0f;
	if (courseProgress < meterPhaseEnd)
	{
		// The opening 5% covers the first kilometre in readable metre steps.
		displayDistanceKm = courseProgress / meterPhaseEnd;
	}
	else
	{
		// Cubic acceleration keeps the unit transition readable, then makes the
		// displayed distance surge toward the pole-to-pole arcade-scale goal.
		const float acceleratedProgress =
			(courseProgress - meterPhaseEnd) / (1.0f - meterPhaseEnd);
		displayDistanceKm = 1.0f
			+ acceleratedProgress * acceleratedProgress * acceleratedProgress
				* (finalDistanceKm - 1.0f);
	}
	return displayDistanceKm * 1000.0f;
}

void PolarLevel::DrawStartMenu()
{
	const std::string title = "POLAR RUNNER";
	const std::string subtitle = "SOUTH POLE  ->  NORTH POLE";
	const std::string start = "ENTER : START";
	const std::string quit = "ESC : QUIT";
	const int centerY = screenHeight / 2;
	Craft::Renderer::Get().Submit(title,
		Craft::Vector2(screenWidth / 2 - static_cast<int>(title.size()) / 2,
			centerY - 6), Craft::Color::BrightWhite, 3000);
	Craft::Renderer::Get().Submit(" _~_ ",
		Craft::Vector2(screenWidth / 2 - 2, centerY - 4),
		Craft::Color::Cyan, 3000);
	Craft::Renderer::Get().Submit("(o o)",
		Craft::Vector2(screenWidth / 2 - 2, centerY - 3),
		Craft::Color::BrightWhite, 3000);
	Craft::Renderer::Get().Submit("/ V \\",
		Craft::Vector2(screenWidth / 2 - 2, centerY - 2),
		Craft::Color::BrightWhite, 3000);
	Craft::Renderer::Get().Submit(subtitle,
		Craft::Vector2(screenWidth / 2 - static_cast<int>(subtitle.size()) / 2,
			centerY), Craft::Color::Cyan, 3000);
	Craft::Renderer::Get().Submit(start,
		Craft::Vector2(screenWidth / 2 - static_cast<int>(start.size()) / 2,
			centerY + 2), Craft::Color::Yellow, 3000);
	Craft::Renderer::Get().Submit(quit,
		Craft::Vector2(screenWidth / 2 - static_cast<int>(quit.size()) / 2,
			centerY + 4), Craft::Color::BrightWhite, 3000);
}

void PolarLevel::DrawPauseMenu()
{
	const char* items[] = { "RESUME", "RETRY", "QUIT" };
	const std::string title = "PAUSED";
	const int centerY = screenHeight / 2;
	Craft::Renderer::Get().Submit(title,
		Craft::Vector2(screenWidth / 2 - static_cast<int>(title.size()) / 2,
			centerY - 3), Craft::Color::Yellow, 3000);
	for (int index = 0; index < static_cast<int>(MenuItem::Length); ++index)
	{
		const bool selected = index == static_cast<int>(selectedMenuItem);
		const std::string line = std::string(selected ? "> " : "  ")
			+ items[index] + (selected ? " <" : "");
		Craft::Renderer::Get().Submit(line,
			Craft::Vector2(screenWidth / 2 - static_cast<int>(line.size()) / 2,
				centerY - 1 + index),
			selected ? Craft::Color::Cyan : Craft::Color::BrightWhite, 3000);
	}
	Craft::Renderer::Get().Submit("UP/DOWN + ENTER   ESC : RESUME",
		Craft::Vector2(screenWidth / 2 - 16, centerY + 4),
		Craft::Color::BrightWhite, 3000);
}

void PolarLevel::Draw()
{
	if (state == State::StartMenu)
	{
		DrawSkyAndHorizon();
		DrawStartMenu();
		return;
	}

	DrawSkyAndHorizon();
	DrawPerspectiveRoad();
	Level::Draw();
	DrawHud();
	if (state == State::PauseMenu)
	{
		DrawPauseMenu();
	}
}
