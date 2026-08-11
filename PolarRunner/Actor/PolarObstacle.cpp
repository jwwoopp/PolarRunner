#include "PolarObstacle.h"

#include <Level/PolarLevel.h>
#include <Render/Renderer.h>

PolarObstacle::PolarObstacle(float horizontalPosition, float distance, ObstacleType type)
	: Actor("", Craft::Vector2::Zero, Craft::Color::Yellow),
	  horizontalPosition(horizontalPosition), distance(distance), obstacleType(type)
{
}

void PolarObstacle::Tick(float deltaTime)
{
	Actor::Tick(deltaTime);
	std::shared_ptr<PolarLevel> level
		= std::dynamic_pointer_cast<PolarLevel>(GetOwner());
	if (level && level->IsPlaying())
	{
		distance -= level->GetRunSpeed() * deltaTime;
	}
}

void PolarObstacle::Draw()
{
	std::shared_ptr<PolarLevel> level
		= std::dynamic_pointer_cast<PolarLevel>(GetOwner());
	if (!level || distance < -3.0f || distance > level->GetViewDistance())
	{
		return;
	}

	const int y = level->DistanceToScreenY(distance);
	const int x = level->GetRoadScreenX(horizontalPosition, y);
	const float closeness = 1.0f - distance / level->GetViewDistance();
	std::string image;
	Craft::Color obstacleColor = Craft::Color::Yellow;
	if (obstacleType == ObstacleType::LowSpike)
	{
		image = closeness > 0.80f ? "^^^^" : (closeness > 0.55f ? "^^" : "^");
	}
	else
	{
		image = closeness > 0.80f ? "########" : (closeness > 0.55f ? "#####" : "###");
		obstacleColor = Craft::Color::Cyan;
	}

	Craft::Renderer::Get().Submit(image,
		Craft::Vector2(x - static_cast<int>(image.size()) / 2, y),
		obstacleColor, y + 10);
}
