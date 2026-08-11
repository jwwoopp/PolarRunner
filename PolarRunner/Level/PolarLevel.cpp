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
	const auto add = [this](Lane lane, float distance, ObstacleType type)
	{
		obstacles.emplace_back(SpawnActor<PolarObstacle>(lane, distance, type));
	};

	// 1. 점프 학습
	add(Lane::Center, 55.0f, ObstacleType::LowSpike);
	// 2. 좌우 레인 변경 학습
	add(Lane::Left, 95.0f, ObstacleType::IceWall);
	add(Lane::Right, 125.0f, ObstacleType::IceWall);
	// 3. 중앙으로 이동한 뒤 점프해야 하는 판단 패턴
	add(Lane::Left, 165.0f, ObstacleType::IceWall);
	add(Lane::Center, 165.0f, ObstacleType::LowSpike);
	add(Lane::Right, 165.0f, ObstacleType::IceWall);
	// 4. 두 레인을 막아 남은 한 레인을 찾게 하는 패턴
	add(Lane::Left, 215.0f, ObstacleType::IceWall);
	add(Lane::Center, 215.0f, ObstacleType::IceWall);
	// 5. 레인 변경 직후 점프
	add(Lane::Right, 250.0f, ObstacleType::IceWall);
	add(Lane::Center, 268.0f, ObstacleType::LowSpike);
	// 6. 속도가 빨라진 뒤 중앙 유도와 연속 전환
	add(Lane::Left, 310.0f, ObstacleType::IceWall);
	add(Lane::Center, 310.0f, ObstacleType::LowSpike);
	add(Lane::Right, 310.0f, ObstacleType::IceWall);
	add(Lane::Left, 350.0f, ObstacleType::IceWall);
	add(Lane::Right, 366.0f, ObstacleType::IceWall);
	add(Lane::Center, 395.0f, ObstacleType::LowSpike);
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
		if (obstacle->HasBeenChecked() || obstacle->GetDistance() > 2.5f)
		{
			continue;
		}
		obstacle->MarkChecked();
		const bool sameLane = obstacle->GetLane() == player->GetLane();
		const bool clearedLowSpike =
			obstacle->GetObstacleType() == ObstacleType::LowSpike
			&& player->IsAboveObstacle();
		if (obstacle->GetDistance() >= -2.0f
			&& sameLane && !clearedLowSpike)
		{
			state = State::Crashed;
			return;
		}
	}
}

int PolarLevel::DistanceToScreenY(float distance) const
{
	const float normalized = std::clamp(1.0f - distance / viewDistance, 0.0f, 1.0f);
	const float perspective = std::pow(normalized, 1.55f);
	return horizonY + static_cast<int>(perspective * (playerScreenY - horizonY));
}

int PolarLevel::GetRoadHalfWidth(int screenY) const
{
	const float normalized = std::clamp(
		static_cast<float>(screenY - horizonY) / (playerScreenY - horizonY),
		0.0f, 1.0f);
	const int nearHalfWidth = screenWidth / 2 - 3;
	return 2 + static_cast<int>(normalized * (nearHalfWidth - 2));
}

int PolarLevel::GetLaneScreenX(Lane lane, int screenY) const
{
	return GetLaneScreenX(static_cast<float>(static_cast<int>(lane)), screenY);
}

int PolarLevel::GetLaneScreenX(float lanePosition, int screenY) const
{
	const int centerX = screenWidth / 2;
	const int halfWidth = GetRoadHalfWidth(screenY);
	const float laneOffset = lanePosition * halfWidth * 0.55f;
	return centerX + static_cast<int>(laneOffset);
}

void PolarLevel::UpdateRunSpeed()
{
	if (traveledDistance < 70.0f) runSpeed = 12.0f;
	else if (traveledDistance < 150.0f) runSpeed = 14.0f;
	else if (traveledDistance < 220.0f) runSpeed = 16.0f;
	else runSpeed = 18.0f;
}

void PolarLevel::DrawPerspectiveRoad()
{
	const int centerX = screenWidth / 2;
	for (int y = horizonY; y <= screenHeight - 1; ++y)
	{
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
		<< "  SPEED: " << runSpeed;
	Craft::Renderer::Get().Submit(hud.str(), Craft::Vector2(1, 0),
		Craft::Color::BrightWhite, 1000);
	Craft::Renderer::Get().Submit("LEFT/RIGHT: Lane   SPACE: Jump   ESC: Quit",
		Craft::Vector2(1, 1), Craft::Color::Cyan, 1000);

	if (state == State::Crashed)
	{
		Craft::Renderer::Get().Submit("CRASH!   R : Retry",
			Craft::Vector2(screenWidth / 2 - 9, screenHeight / 2),
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
