#include "PolarPlayer.h"

#include <Engine/Engine.h>
#include <Input/Input.h>
#include <Level/PolarLevel.h>
#include <Render/Renderer.h>
#include <Windows.h>
#include <algorithm>
#include <cmath>

PolarPlayer::PolarPlayer()
	: Actor("", Craft::Vector2::Zero, Craft::Color::Cyan)
{
}

void PolarPlayer::Tick(float deltaTime)
{
	Actor::Tick(deltaTime);
	std::shared_ptr<PolarLevel> level
		= std::dynamic_pointer_cast<PolarLevel>(GetOwner());
	if (!level || !level->IsPlaying())
	{
		return;
	}

	if (Craft::Input::Get().GetKeyDown(VK_LEFT)
		&& lane != Lane::Left)
	{
		laneMoveStart = visualLanePosition;
		laneMoveElapsed = 0.0f;
		lane = static_cast<Lane>(static_cast<int>(lane) - 1);
	}
	else if (Craft::Input::Get().GetKeyDown(VK_RIGHT)
		&& lane != Lane::Right)
	{
		laneMoveStart = visualLanePosition;
		laneMoveElapsed = 0.0f;
		lane = static_cast<Lane>(static_cast<int>(lane) + 1);
	}

	const float targetLane = static_cast<float>(static_cast<int>(lane));
	if (visualLanePosition != targetLane)
	{
		laneMoveElapsed += deltaTime;
		const float progress = (std::min)(laneMoveElapsed / laneMoveDuration, 1.0f);
		visualLanePosition = laneMoveStart + (targetLane - laneMoveStart) * progress;
	}

	jumpInputBufferRemaining
		= (std::max)(0.0f, jumpInputBufferRemaining - deltaTime);
	if (Craft::Input::Get().GetKeyDown(VK_SPACE))
	{
		if (!isJumping)
		{
			isJumping = true;
			jumpTimer = 0.0f;
		}
		else
		{
			jumpInputBufferRemaining = jumpInputBufferDuration;
		}
	}

	if (isJumping)
	{
		jumpTimer += deltaTime;
		if (jumpTimer >= jumpDuration)
		{
			if (jumpInputBufferRemaining > 0.0f)
			{
				jumpTimer = 0.0f;
				jumpInputBufferRemaining = 0.0f;
			}
			else
			{
				jumpTimer = 0.0f;
				isJumping = false;
			}
		}
	}
}

bool PolarPlayer::IsAboveObstacle() const
{
	return GetJumpScreenOffset() >= 2;
}

int PolarPlayer::GetJumpScreenOffset() const
{
	if (!isJumping)
	{
		return 0;
	}
	const float progress = jumpTimer / jumpDuration;
	return static_cast<int>(std::sin(progress * 3.14159265f) * 4.0f);
}

void PolarPlayer::Draw()
{
	std::shared_ptr<PolarLevel> level
		= std::dynamic_pointer_cast<PolarLevel>(GetOwner());
	if (!level)
	{
		return;
	}

	const int y = level->GetPlayerScreenY() - GetJumpScreenOffset();
	const int x = level->GetLaneScreenX(
		visualLanePosition, level->GetPlayerScreenY());
	if (isJumping)
	{
		Craft::Renderer::Get().Submit(" _~_ ", Craft::Vector2(x - 2, y - 4), Craft::Color::Cyan, 100);
		Craft::Renderer::Get().Submit(" (o o)", Craft::Vector2(x - 3, y - 3), Craft::Color::BrightWhite, 100);
		Craft::Renderer::Get().Submit(" / V \\", Craft::Vector2(x - 3, y - 2), Craft::Color::BrightWhite, 100);
		Craft::Renderer::Get().Submit("/( _ )\\", Craft::Vector2(x - 3, y - 1), Craft::Color::BrightWhite, 100);
		Craft::Renderer::Get().Submit("  ^ ^", Craft::Vector2(x - 2, y), Craft::Color::Yellow, 100);
	}
	else
	{
		Craft::Renderer::Get().Submit(" (o)___", Craft::Vector2(x - 3, y - 1), Craft::Color::Cyan, 100);
		Craft::Renderer::Get().Submit("____<(_____)", Craft::Vector2(x - 6, y), Craft::Color::BrightWhite, 100);
	}
}
