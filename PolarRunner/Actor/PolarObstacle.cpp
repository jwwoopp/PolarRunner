#include "PolarObstacle.h"

#include <Level/PolarLevel.h>
#include <Render/Renderer.h>
#include <algorithm>

PolarObstacle::PolarObstacle(float horizontalPosition, float distance,
	ObstacleType type, float horizontalHalfWidth)
	: Actor("", Craft::Vector2::Zero, Craft::Color::Yellow),
	  horizontalPosition(horizontalPosition), distance(distance),
	  previousDistance(distance), obstacleType(type),
	  horizontalHalfWidth(horizontalHalfWidth),
	  visualVariant(static_cast<int>(distance / 10.0f
		  + (horizontalPosition + 1.0f) * 7.0f) % 3)
{
}

ScreenBounds PolarObstacle::GetIceWallScreenBounds() const
{
	ScreenBounds bounds;
	std::shared_ptr<PolarLevel> level
		= std::dynamic_pointer_cast<PolarLevel>(GetOwner());
	if (!level)
	{
		return bounds;
	}

	const int y = level->DistanceToScreenY(distance);
	// Draw()가 사용하는 것과 완전히 같은 X를 사용합니다. 커브에서는 Y행마다
	// 도로 중심이 달라지므로 플레이어 Y행에서 다시 투영하면 화면에 보이는
	// 벽과 충돌용 벽의 위치가 서로 달라집니다.
	const int x = level->GetRoadScreenX(horizontalPosition, y);
	const float closeness = 1.0f - distance / level->GetViewDistance();

	// Draw()의 근접 단계(closeness 기준)와 같은 폭을 쓰되, 위로 갈수록
	// 좁아지는 지붕 행은 제외하고 일정 폭으로 그려지는 몸통 행만 씁니다.
	int bodyWidth = visualVariant == 0 ? 2 : 3;
	int bodyRowSpan = 0;
	if (closeness > 0.72f)
	{
		bodyWidth = 8;
		bodyRowSpan = 2;
	}
	else if (closeness > 0.30f)
	{
		bodyWidth = 6;
		bodyRowSpan = 1;
	}

	// Draw()는 x - row.size() / 2에서 문자열을 시작합니다. 짝수 너비를
	// x +/- halfWidth로 계산하면 실제 출력보다 오른쪽이 한 칸 더 넓어져
	// 보이지 않는 충돌 칸이 생기므로 문자열 길이 그대로 끝 좌표를 구합니다.
	bounds.left = x - bodyWidth / 2;
	bounds.right = bounds.left + bodyWidth - 1;
	bounds.top = y - bodyRowSpan;
	bounds.bottom = y;
	return bounds;
}

void PolarObstacle::Tick(float deltaTime)
{
	Actor::Tick(deltaTime);
	std::shared_ptr<PolarLevel> level
		= std::dynamic_pointer_cast<PolarLevel>(GetOwner());
	if (level && level->IsPlaying())
	{
		previousDistance = distance;
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
	else if (obstacleType == ObstacleType::Puddle)
	{
		obstacleColor = Craft::Color::Blue;
		image = closeness > 0.80f ? "~~~~~~~~"
			: (closeness > 0.55f ? "~~~~" : "~");
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
		if (closeness > 0.72f)
		{
			if (visualVariant == 0)
			{
				drawCentered("  /\\/\\  ", -5);
				drawCentered(" /####\\ ", -4);
				drawCentered("/######\\", -3);
				drawCentered("|######|", -2);
				drawCentered("|######|", -1);
				image = "|######|";
			}
			else if (visualVariant == 1)
			{
				drawCentered(" /\\  /\\ ", -5);
				drawCentered("/##\\/##\\", -4);
				drawCentered("|######|", -3);
				drawCentered("|######|", -2);
				drawCentered("|######|", -1);
				image = "|######|";
			}
			else
			{
				drawCentered("   /\\   ", -5);
				drawCentered("  /##\\  ", -4);
				drawCentered(" /####\\ ", -3);
				drawCentered("/######\\", -2);
				drawCentered("|##/\\##|", -1);
				image = "|_/  \\_|";
			}
		}
		else if (closeness > 0.30f)
		{
			const std::string roof = visualVariant == 0 ? " /\\/\\ "
				: (visualVariant == 1 ? "/\\  /\\" : "  /\\  ");
			drawCentered(roof, -2);
			drawCentered(visualVariant == 2 ? " /##\\ " : "/####\\", -1);
			image = visualVariant == 2 ? "|_/\\_|" : "|####|";
		}
		else
		{
			image = visualVariant == 0 ? "/\\"
				: (visualVariant == 1 ? "/^\\" : "/|\\");
		}
	}

	Craft::Renderer::Get().Submit(image,
		Craft::Vector2(x - static_cast<int>(image.size()) / 2, y),
		obstacleColor, y + 10);
}
