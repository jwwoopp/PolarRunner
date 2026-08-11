#include "PolarObstacle.h"

#include <Level/PolarLevel.h>
#include <Render/Renderer.h>
#include <algorithm>

PolarObstacle::PolarObstacle(float horizontalPosition, float distance,
	ObstacleType type, float horizontalHalfWidth)
	: Actor("", Craft::Vector2::Zero, Craft::Color::Yellow),
	  horizontalPosition(horizontalPosition), distance(distance), obstacleType(type),
	  horizontalHalfWidth(horizontalHalfWidth),
	  visualVariant(static_cast<int>(distance / 10.0f
		  + (horizontalPosition + 1.0f) * 7.0f) % 3)
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
	if (obstacleType == ObstacleType::BrokenBridge)
	{
		const auto drawGapRow = [level, y](int rowY,
			const std::string& pattern, Craft::Color color)
		{
			const int centerX = level->GetRoadCenterX(rowY);
			const int halfWidth = level->GetRoadHalfWidth(rowY);
			const int gapWidth = (std::max)(3, halfWidth * 2 - 1);
			std::string row;
			row.reserve(gapWidth);
			for (int index = 0; index < gapWidth; ++index)
			{
				row += pattern[index % pattern.size()];
			}
			Craft::Renderer::Get().Submit(row,
				Craft::Vector2(centerX - gapWidth / 2, rowY), color, y + 50);
		};

		if (closeness > 0.80f)
		{
			drawGapRow(y - 2, "/\\", Craft::Color::BrightWhite);
			drawGapRow(y - 1, "~-", Craft::Color::Blue);
			drawGapRow(y, "\\/", Craft::Color::BrightWhite);
		}
		else if (closeness > 0.55f)
		{
			drawGapRow(y - 1, "/\\", Craft::Color::BrightWhite);
			drawGapRow(y, "~-", Craft::Color::Blue);
			drawGapRow(y + 1, "\\/", Craft::Color::BrightWhite);
		}
		else
		{
			drawGapRow(y, "^", Craft::Color::BrightWhite);
		}
		return;
	}
	if (obstacleType == ObstacleType::LowSpike)
	{
		image = closeness > 0.80f ? "^^^^" : (closeness > 0.55f ? "^^" : "^");
	}
	else
	{
		obstacleColor = Craft::Color::Cyan;
		const auto drawCentered = [x, y, obstacleColor](
			const std::string& row, int yOffset)
		{
			Craft::Renderer::Get().Submit(row,
				Craft::Vector2(x - static_cast<int>(row.size()) / 2, y + yOffset),
				obstacleColor, y + 10);
		};
		if (closeness > 0.80f)
		{
			if (visualVariant == 0)
			{
				drawCentered("/------\\", -2);
				drawCentered("|######|", -1);
				image = "|######|";
			}
			else
			{
				drawCentered("/^^^^\\", -2);
				drawCentered("|####|", -1);
				image = "\\____/";
			}
		}
		else if (closeness > 0.55f)
		{
			drawCentered(visualVariant == 0 ? "/---\\" : "/^^\\", -1);
			image = visualVariant == 0 ? "|###|" : "\\__/";
		}
		else
		{
			image = visualVariant == 0 ? "[]"
				: (visualVariant == 1 ? "I" : "<>");
		}
	}

	Craft::Renderer::Get().Submit(image,
		Craft::Vector2(x - static_cast<int>(image.size()) / 2, y),
		obstacleColor, y + 10);
}
