#include "PolarStar.h"

#include <Level/PolarLevel.h>
#include <Render/Renderer.h>

PolarStar::PolarStar(float horizontalPosition, float distance)
	: Actor("", Craft::Vector2::Zero, Craft::Color::Yellow),
	  horizontalPosition(horizontalPosition), distance(distance),
	  previousDistance(distance)
{
}

void PolarStar::Tick(float deltaTime)
{
	Actor::Tick(deltaTime);
	std::shared_ptr<PolarLevel> level
		= std::dynamic_pointer_cast<PolarLevel>(GetOwner());
	if (level && level->IsPlaying())
	{
		previousDistance = distance;
		distance -= level->GetRunSpeed() * deltaTime;
		if (distance < -2.0f)
		{
			Destroy();
		}
	}
}

void PolarStar::Draw()
{
	if (isCollected)
	{
		return;
	}

	std::shared_ptr<PolarLevel> level
		= std::dynamic_pointer_cast<PolarLevel>(GetOwner());
	if (!level || distance < -2.0f || distance > level->GetViewDistance())
	{
		return;
	}

	const int y = level->DistanceToScreenY(distance);
	const int x = level->GetRoadScreenX(horizontalPosition, y);
	const float closeness = 1.0f - distance / level->GetViewDistance();
	const std::string image = closeness > 0.72f
		? "<*>" : (closeness > 0.38f ? "+" : ".");
	Craft::Renderer::Get().Submit(image,
		Craft::Vector2(x - static_cast<int>(image.size()) / 2, y),
		Craft::Color::Yellow, y + 20);
}

void PolarStar::Collect()
{
	if (isCollected)
	{
		return;
	}
	isCollected = true;
	Destroy();
}
