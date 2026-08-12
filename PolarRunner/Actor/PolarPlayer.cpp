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

	const Craft::Input& input = Craft::Input::Get();
	const bool jumpPressed = input.GetKeyDown(VK_SPACE);
	const float horizontalInput =
		(input.GetKey(VK_RIGHT) ? 1.0f : 0.0f)
		- (input.GetKey(VK_LEFT) ? 1.0f : 0.0f);
	// Narrow ice has lower friction: steering builds more slowly and releasing
	// the key preserves sideways momentum long enough to require correction.
	const bool isOnNarrowIce = level->IsOnNarrowIcePath();
	const float horizontalResponse = isOnNarrowIce
		? (horizontalInput == 0.0f ? 3.5f : 7.0f)
		: (horizontalInput == 0.0f ? 10.0f : 11.0f);
	const float horizontalBlend = (std::min)(horizontalResponse * deltaTime, 1.0f);
	horizontalVelocity += (horizontalInput * horizontalSpeed - horizontalVelocity)
		* horizontalBlend;

	const float horizontalMin = level->GetPlayerHorizontalMin();
	const float horizontalMax = level->GetPlayerHorizontalMax();
	horizontalPosition = std::clamp(
		horizontalPosition + horizontalVelocity * deltaTime,
		horizontalMin, horizontalMax);
	if (horizontalPosition == horizontalMin || horizontalPosition == horizontalMax)
	{
		horizontalVelocity = 0.0f;
	}

	const float longitudinalInput =
		(input.GetKey(VK_DOWN) ? 1.0f : 0.0f)
		- (input.GetKey(VK_UP) ? 1.0f : 0.0f);
	// Lock forward/back movement while airborne. Moving the collision row during
	// a jump let UP + SPACE extend bridge clearance without extending jump time.
	if (!isJumping && !jumpPressed)
	{
		longitudinalScreenOffset = std::clamp(
			longitudinalScreenOffset
				+ longitudinalInput * longitudinalSpeed * deltaTime,
			-8.0f, 4.0f);
	}

	jumpInputBufferRemaining
		= (std::max)(0.0f, jumpInputBufferRemaining - deltaTime);
	if (jumpPressed)
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
	return GetJumpHeight() >= 0.4f;
}

float PolarPlayer::GetJumpHeight() const
{
	if (!isJumping)
	{
		return 0.0f;
	}
	const float progress = jumpTimer / jumpDuration;
	return std::sin(progress * 3.14159265f);
}

int PolarPlayer::GetJumpScreenOffset() const
{
	return static_cast<int>(GetJumpHeight() * 5.0f);
}

void PolarPlayer::Draw()
{
	std::shared_ptr<PolarLevel> level
		= std::dynamic_pointer_cast<PolarLevel>(GetOwner());
	if (!level)
	{
		return;
	}

	const int screenY = level->GetPlayerScreenY();
	const int y = screenY - GetJumpScreenOffset();
	const int x = level->GetRoadScreenX(horizontalPosition, screenY);
	if (isJumping)
	{
		Craft::Renderer::Get().Submit(" _~_ ", Craft::Vector2(x - 2, y - 4), Craft::Color::Cyan, 100);
		Craft::Renderer::Get().Submit(" (o o)", Craft::Vector2(x - 3, y - 3), Craft::Color::BrightWhite, 100);
		Craft::Renderer::Get().Submit(" / V \\", Craft::Vector2(x - 3, y - 2), Craft::Color::BrightWhite, 100);
		Craft::Renderer::Get().Submit("/( _ )\\", Craft::Vector2(x - 3, y - 1), Craft::Color::BrightWhite, 100);
		Craft::Renderer::Get().Submit("^ ^", Craft::Vector2(x - 1, y), Craft::Color::Yellow, 100);
	}
	else
	{
		Craft::Renderer::Get().Submit(" <(o)___", Craft::Vector2(x - 4, y - 1), Craft::Color::BrightWhite, 100);
		Craft::Renderer::Get().Submit("____(_____)", Craft::Vector2(x - 6, y), Craft::Color::BrightWhite, 100);
	}
}
