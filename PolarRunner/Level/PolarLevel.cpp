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
#include <sstream>

void PolarLevel::OnInitialized()
{
	Level::OnInitialized();
	screenWidth = Craft::Engine::Get().GetWidth();
	screenHeight = Craft::Engine::Get().GetHeight();
	horizonY = 4;
	playerScreenY = screenHeight - 6;
	player = SpawnActor<PolarPlayer>();
	BuildRoadCourse();
	BuildTestCourse();
}

void PolarLevel::BuildRoadCourse()
{
	// Distance, horizontal curve offset, and width scale. Closely spaced slices
	// create short, readable transitions between terrain types.
	roadSlices = {
		{ 0.0f, 0.0f, 1.00f, TerrainType::Snowfield },
		{ 150.0f, -0.40f, 0.85f, TerrainType::LeftCoast },
		{ 300.0f, 0.0f, 0.70f, TerrainType::Snowfield },
		{ 450.0f, 0.40f, 0.85f, TerrainType::RightCoast },
		{ 600.0f, 0.0f, 1.35f, TerrainType::Snowfield },
		{ 750.0f, -0.30f, 1.00f, TerrainType::LeftCoast },
		{ 850.0f, 0.30f, 0.90f, TerrainType::RightCoast },
		{ 1000.0f, 0.0f, 1.20f, TerrainType::Snowfield }
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

	// Straight: learn one mechanic at a time before the road begins to turn.
	spike(0.0f, 55.0f);
	// 70-145: the first left curve is intentionally empty.
	// Late left curve: introduce one wall only after steering practice.
	wall(-0.42f, 170.0f);
	// 215-290: the first part of the right curve is intentionally empty.
	// Late right curve: two joined walls leave a broad outside corridor.
	wall(-0.46f, 310.0f);
	wall(0.0f, 310.0f);
	// Straight: first composite pattern, with a forced centre jump.
	wall(-0.52f, 365.0f);
	spike(0.0f, 365.0f);
	wall(0.52f, 365.0f);
	// Gentle S curve: widely spaced single decisions before the goal.
	wall(0.42f, 420.0f);
	spike(-0.18f, 470.0f);

	// Continue the obstacle course through the second half of the 1000m run.
	spike(0.12f, 540.0f);
	wall(-0.45f, 610.0f);
	wall(0.35f, 680.0f);
	spike(-0.20f, 755.0f);
	wall(-0.50f, 825.0f);
	wall(0.05f, 825.0f);
	spike(0.25f, 900.0f);
	wall(0.40f, 955.0f);
}

void PolarLevel::Tick(float deltaTime)
{
	HandleMenuInput();
	if (!IsPlaying())
	{
		return;
	}

	UpdateCurve(deltaTime);
	UpdateRunSpeed();
	Level::Tick(deltaTime);
	traveledDistance += runSpeed * deltaTime;
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
		// Once an obstacle has visibly passed the player's current row it must
		// not become dangerous again if the player moves backward.
		if (obstacleScreenY > currentPlayerScreenY + 1
			|| obstacle->GetDistance() < -2.0f)
		{
			obstacle->MarkChecked();
			continue;
		}

		const int playerScreenX = GetRoadScreenX(
			player->GetHorizontalPosition(), currentPlayerScreenY);
		const int obstacleScreenX = GetRoadScreenX(
			obstacle->GetHorizontalPosition(), obstacleScreenY);
		const int collisionRoadScale = (std::max)(0,
			GetRoadHalfWidth(currentPlayerScreenY) - 6);
		const float combinedHalfWidth =
			(player->GetHorizontalHalfWidth()
				+ obstacle->GetHorizontalHalfWidth()) * collisionRoadScale;
		const bool horizontalOverlap =
			std::abs(playerScreenX - obstacleScreenX) < combinedHalfWidth;
		// Collision follows the player's movable screen row. A one-row window
		// prevents a fast obstacle from slipping through between rendered rows.
		const bool reachesPlayer =
			std::abs(obstacleScreenY - currentPlayerScreenY) <= 1;
		const bool clearedLowSpike =
			obstacle->GetObstacleType() == ObstacleType::LowSpike
			&& player->IsAboveObstacle();
		if (horizontalOverlap && reachesPlayer && !clearedLowSpike)
		{
			crashedObstacleType = obstacle->GetObstacleType();
			state = State::Crashed;
			return;
		}
	}
}

int PolarLevel::GetPlayerScreenY() const
{
	const int offset = player ? player->GetLongitudinalScreenOffset() : 0;
	return std::clamp(playerScreenY + offset, horizonY + 3, screenHeight - 2);
}

const char* PolarLevel::GetObstacleName(ObstacleType type) const
{
	return type == ObstacleType::IceWall ? "ICE WALL" : "LOW SPIKE";
}

int PolarLevel::DistanceToScreenY(float distance) const
{
	const float normalized = std::clamp(1.0f - distance / viewDistance, 0.0f, 1.0f);
	const float perspective = std::pow(normalized, 1.55f);
	const int roadBottomY = screenHeight - 1;
	return horizonY + static_cast<int>(perspective * (roadBottomY - horizonY));
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
	return std::clamp(static_cast<float>(screenY - horizonY)
		/ (roadBottomY - horizonY), 0.0f, 1.0f);
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

	// A 30% half-width makes the normal road occupy about 60% of the screen.
	const int nearHalfWidth = static_cast<int>(screenWidth * 0.30f);
	const int perspectiveHalfWidth = 2
		+ static_cast<int>(depth * (nearHalfWidth - 2));
	const int maximumVisibleHalfWidth = (std::max)(2,
		(std::min)(slice.centerX - 1, screenWidth - slice.centerX - 2));
	const float widthBlend = depth * depth * (3.0f - 2.0f * depth);
	const float visibleWidth = 1.0f + (slice.width - 1.0f) * widthBlend;
	const int minimumHalfWidth = 2 + static_cast<int>(depth
		* (nearHalfWidth * 0.72f - 2.0f));
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
	const int playerMargin = 6;
	const int usableHalfWidth = (std::max)(0, slice.halfWidth - playerMargin);
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

void PolarLevel::UpdateRunSpeed()
{
	runSpeed = std::abs(curveStrength) > 0.08f ? 14.0f : 16.0f;
}

void PolarLevel::DrawPerspectiveRoad()
{
	const int roadTopY = horizonY;
	const int roadBottomY = screenHeight - 1;
	const RoadSlice topSlice = CalculateRoadSlice(0.0f);
	DrawTerrainRow(roadTopY, topSlice);
	Craft::Vector2 previousLeft(
		topSlice.centerX - topSlice.halfWidth, roadTopY);
	Craft::Vector2 previousRight(
		topSlice.centerX + topSlice.halfWidth, roadTopY);
	Craft::Renderer::Get().Submit("/", previousLeft, Craft::Color::Cyan, 0);
	Craft::Renderer::Get().Submit("\\", previousRight, Craft::Color::Cyan, 0);

	for (int y = roadTopY + 1; y <= roadBottomY; ++y)
	{
		const float depth = static_cast<float>(y - roadTopY)
			/ (roadBottomY - roadTopY);
		const RoadSlice currentSlice = CalculateRoadSlice(depth);
		DrawTerrainRow(y, currentSlice);
		const Craft::Vector2 currentLeft(
			currentSlice.centerX - currentSlice.halfWidth, y);
		const Craft::Vector2 currentRight(
			currentSlice.centerX + currentSlice.halfWidth, y);
		DrawRoadBoundary(previousLeft, currentLeft, true);
		DrawRoadBoundary(previousRight, currentRight, false);
		if ((y - roadTopY) % 3 == 0)
		{
			Craft::Renderer::Get().Submit(".", Craft::Vector2(currentSlice.centerX, y),
				Craft::Color::BrightWhite, 0);
		}
		previousLeft = currentLeft;
		previousRight = currentRight;
	}
}

void PolarLevel::DrawTerrainRow(int y, const RoadSlice& slice)
{
	const int leftX = slice.centerX - slice.halfWidth;
	const int rightX = slice.centerX + slice.halfWidth;
	switch (slice.terrain)
	{
	case TerrainType::LeftCoast:
		DrawOcean(y, 0, leftX - 1);
		DrawSnowSurface(y, rightX + 1, screenWidth - 1);
		break;
	case TerrainType::RightCoast:
		DrawSnowSurface(y, 0, leftX - 1);
		DrawOcean(y, rightX + 1, screenWidth - 1);
		break;
	case TerrainType::Snowfield:
	default:
		DrawSnowSurface(y, 0, leftX - 1);
		DrawSnowSurface(y, rightX + 1, screenWidth - 1);
		break;
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
				Craft::Color::BrightWhite, -10);
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
				Craft::Color::Blue, -10);
		}
	}
}

void PolarLevel::DrawRoadBoundary(const Craft::Vector2& previous,
	const Craft::Vector2& current, bool leftBoundary)
{
	const char* glyph = previous.x == current.x ? "|"
		: ((current.x > previous.x) == leftBoundary ? "\\" : "/");
	// One boundary cell per row prevents a multi-column delta from becoming
	// a horizontal zipper. The previous point is used only to select the slope.
	Craft::Renderer::Get().Submit(glyph, current, Craft::Color::Cyan, 0);
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
		const std::string message = std::string("CRASH: ")
			+ GetObstacleName(crashedObstacleType) + "   R : Retry";
		Craft::Renderer::Get().Submit(message,
			Craft::Vector2(screenWidth / 2 - static_cast<int>(message.size()) / 2,
				screenHeight / 2),
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
	const std::string start = "ENTER : START";
	const std::string quit = "ESC : QUIT";
	const int centerY = screenHeight / 2;
	Craft::Renderer::Get().Submit(title,
		Craft::Vector2(screenWidth / 2 - static_cast<int>(title.size()) / 2,
			centerY - 2), Craft::Color::BrightWhite, 3000);
	Craft::Renderer::Get().Submit(start,
		Craft::Vector2(screenWidth / 2 - static_cast<int>(start.size()) / 2,
			centerY), Craft::Color::Cyan, 3000);
	Craft::Renderer::Get().Submit(quit,
		Craft::Vector2(screenWidth / 2 - static_cast<int>(quit.size()) / 2,
			centerY + 2), Craft::Color::BrightWhite, 3000);
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
	DrawPerspectiveRoad();
	Level::Draw();
	DrawHud();
	if (state == State::StartMenu)
	{
		DrawStartMenu();
	}
	else if (state == State::PauseMenu)
	{
		DrawPauseMenu();
	}
}
