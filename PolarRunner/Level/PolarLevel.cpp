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
	playerScreenY = screenHeight - 3;
	player = SpawnActor<PolarPlayer>();
	BuildTestCourse();
}

void PolarLevel::BuildTestCourse()
{
	// Widths use the same normalized road coordinates as player movement and
	// match the near-camera four-column spike and eight-column wall sprites.
	constexpr float spikeHalfWidth = 0.07f;
	constexpr float wallHalfWidth = 0.13f;
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
}

void PolarLevel::Tick(float deltaTime)
{
	if (Craft::Input::Get().GetKeyDown(VK_ESCAPE))
	{
		Craft::Engine::Get().Quit();
	}
	if (Craft::Input::Get().GetKeyDown('R'))
	{
		Craft::Engine::Get().AddNewLevel<PolarLevel>();
		return;
	}
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

		// DistanceToScreenY clamps passed obstacles to the player's row, so
		// retire them using world distance instead of their clamped screen Y.
		if (obstacle->GetDistance() < -2.0f)
		{
			obstacle->MarkChecked();
			continue;
		}

		const bool horizontalOverlap = std::abs(
			player->GetHorizontalPosition() - obstacle->GetHorizontalPosition())
			< player->GetHorizontalHalfWidth()
				+ obstacle->GetHorizontalHalfWidth();
		// A world-space interval remains reliable at 18 units/s even if the
		// renderer skips the exact player row between two frames.
		const bool reachesPlayer = obstacle->GetDistance() <= 2.0f
			&& obstacle->GetDistance() >= -2.0f;
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

const char* PolarLevel::GetObstacleName(ObstacleType type) const
{
	return type == ObstacleType::IceWall ? "ICE WALL" : "LOW SPIKE";
}

int PolarLevel::DistanceToScreenY(float distance) const
{
	const float normalized = std::clamp(1.0f - distance / viewDistance, 0.0f, 1.0f);
	const float perspective = std::pow(normalized, 1.55f);
	return horizonY + static_cast<int>(perspective * (playerScreenY - horizonY));
}

int PolarLevel::GetRoadCenterX(int screenY) const
{
	const float nearAmount = std::clamp(
		static_cast<float>(screenY - horizonY) / (playerScreenY - horizonY),
		0.0f, 1.0f);
	const float curvePerspective = std::pow(1.0f - nearAmount, 1.6f);
	const float maximumCurveOffset = screenWidth * 0.22f;
	return screenWidth / 2 + static_cast<int>(
		curveStrength * curvePerspective * maximumCurveOffset);
}

int PolarLevel::GetRoadHalfWidth(int screenY) const
{
	const float normalized = std::clamp(
		static_cast<float>(screenY - horizonY) / (playerScreenY - horizonY),
		0.0f, 1.0f);
	const int nearHalfWidth = screenWidth / 2 - 3;
	return 2 + static_cast<int>(normalized * (nearHalfWidth - 2));
}

int PolarLevel::GetRoadScreenX(float horizontalPosition, int screenY) const
{
	const int centerX = GetRoadCenterX(screenY);
	const int halfWidth = GetRoadHalfWidth(screenY);
	const int playerMargin = 6;
	const int usableHalfWidth = (std::max)(0, halfWidth - playerMargin);
	return centerX + static_cast<int>(horizontalPosition * usableHalfWidth);
}

void PolarLevel::UpdateCurve(float deltaTime)
{
	float targetStrength = 0.0f;
	if (traveledDistance >= 70.0f && traveledDistance < 145.0f)
	{
		targetStrength = -0.60f;
	}
	else if (traveledDistance >= 145.0f && traveledDistance < 190.0f)
	{
		targetStrength = -0.56f;
	}
	else if (traveledDistance >= 215.0f && traveledDistance < 290.0f)
	{
		targetStrength = 0.56f;
	}
	else if (traveledDistance >= 290.0f && traveledDistance < 330.0f)
	{
		targetStrength = 0.60f;
	}
	else if (traveledDistance >= 390.0f && traveledDistance < 425.0f)
	{
		targetStrength = -0.52f;
	}
	else if (traveledDistance >= 425.0f && traveledDistance < 465.0f)
	{
		targetStrength = 0.52f;
	}

	const float blend = (std::min)(deltaTime * 2.8f, 1.0f);
	curveStrength += (targetStrength - curveStrength) * blend;
	if (std::abs(curveStrength) < 0.01f && targetStrength == 0.0f)
	{
		curveStrength = 0.0f;
	}
}

const char* PolarLevel::GetCurveDirection() const
{
	if (curveStrength < -0.08f) return "LEFT";
	if (curveStrength > 0.08f) return "RIGHT";
	return "STRAIGHT";
}

void PolarLevel::UpdateRunSpeed()
{
	runSpeed = std::abs(curveStrength) > 0.08f ? 12.0f : 14.0f;
}

void PolarLevel::DrawPerspectiveRoad()
{
	for (int y = horizonY; y <= screenHeight - 1; ++y)
	{
		const int centerX = GetRoadCenterX(y);
		const int halfWidth = GetRoadHalfWidth(y);
		Craft::Renderer::Get().Submit("/", Craft::Vector2(centerX - halfWidth, y),
			Craft::Color::Cyan, 0);
		Craft::Renderer::Get().Submit("\\", Craft::Vector2(centerX + halfWidth, y),
			Craft::Color::Cyan, 0);
		if ((y - horizonY) % 3 == 0)
		{
			Craft::Renderer::Get().Submit(".", Craft::Vector2(centerX, y),
				Craft::Color::BrightWhite, 0);
		}
	}
}

void PolarLevel::DrawHud()
{
	std::ostringstream hud;
	hud << std::fixed << std::setprecision(1)
		<< "POLAR RUNNER  DIST: " << traveledDistance
		<< " / " << courseDistance
		<< "  SPEED: " << runSpeed
		<< "  ROAD: " << GetCurveDirection();
	Craft::Renderer::Get().Submit(hud.str(), Craft::Vector2(1, 0),
		Craft::Color::BrightWhite, 1000);
	Craft::Renderer::Get().Submit("LEFT/RIGHT: Glide   SPACE: Jump   ESC: Quit",
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

void PolarLevel::Draw()
{
	DrawPerspectiveRoad();
	Level::Draw();
	DrawHud();
}
