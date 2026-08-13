#include "PolarLevel.h"

#include <Actor/Player.h>
#include <Math/Color.h>
#include <Render/Renderer.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

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

void PolarLevel::DrawNarrowPathWarningSign()
{
	constexpr float signCoursePosition = 720.0f;
	const float distanceToSign = signCoursePosition - traveledDistance;
	if (distanceToSign < 0.0f || distanceToSign > viewDistance)
	{
		return;
	}

	const int y = DistanceToScreenY(distanceToSign);
	const int preferredX = GetRoadCenterX(y) + GetRoadHalfWidth(y) + 2;
	const int x = std::clamp(preferredX, 0, screenWidth - 7);
	Craft::Renderer::Get().Submit("[!]", Craft::Vector2(x, y - 1),
		Craft::Color::Yellow, 50);
	Craft::Renderer::Get().Submit("SLIP", Craft::Vector2(x, y),
		Craft::Color::Yellow, 50);
}

void PolarLevel::DrawSnowfieldRow(int y, float depth, const RoadSlice& slice)
{
	DrawRoadEdges(y, slice, &PolarLevel::DrawSnowSurface, &PolarLevel::DrawSnowSurface,
		depth, 0.55f, "|", "/", "|", "\\",
		Craft::Color::BrightWhite, Craft::Color::BrightWhite);
}

void PolarLevel::DrawCoastRow(int y, float depth, const RoadSlice& slice)
{
	DrawRoadEdges(y, slice, &PolarLevel::DrawOcean, &PolarLevel::DrawSnowSurface,
		depth, 0.45f, ":", "~", "\\", "\\",
		Craft::Color::Blue, Craft::Color::BrightWhite);
}

void PolarLevel::DrawCanyonRow(int y, float depth, const RoadSlice& slice)
{
	DrawRoadEdges(y, slice, &PolarLevel::DrawCanyonWall, &PolarLevel::DrawCanyonWall,
		depth, 0.5f, "|", "#", "|", "#",
		Craft::Color::Cyan, Craft::Color::Cyan);
}

void PolarLevel::DrawNarrowIcePathRow(int y, float depth, const RoadSlice& slice)
{
	// Perspective rails make the strip read as a bridge rather than a tunnel.
	DrawRoadEdges(y, slice, &PolarLevel::DrawOcean, &PolarLevel::DrawOcean,
		depth, 0.5f, "/", "/", "\\", "\\",
		Craft::Color::BrightWhite, Craft::Color::BrightWhite, 1);

	// Keep the intact ice quiet so the dense blue gap is immediately readable.
	// (일반 Draw*Row 골격과 다른, 이 지형만의 얼음 격자 스티플이라 따로 그립니다.)
	const int leftX = slice.centerX - slice.halfWidth;
	const int rightX = slice.centerX + slice.halfWidth;
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
	DrawRoadEdges(y, slice, &PolarLevel::DrawOcean, &PolarLevel::DrawOcean,
		depth, 0.5f, "|", "/", "|", "\\",
		Craft::Color::Cyan, Craft::Color::Cyan);
	// 도로 안쪽 깨진 빙판은 공용 골격에 없는 이 지형만의 내부 채우기입니다.
	const int leftX = slice.centerX - slice.halfWidth;
	const int rightX = slice.centerX + slice.halfWidth;
	DrawBrokenIce(y, leftX + 1, rightX - 1);
}

void PolarLevel::DrawResearchBaseRow(int y, float depth, const RoadSlice& slice)
{
	DrawRoadEdges(y, slice, &PolarLevel::DrawResearchBase, &PolarLevel::DrawResearchBase,
		depth, 0.45f, "|", "[", "|", "]",
		Craft::Color::BrightWhite, Craft::Color::BrightWhite);
}

void PolarLevel::DrawRoadEdges(int y, const RoadSlice& slice,
	void (PolarLevel::*leftFill)(int, int, int),
	void (PolarLevel::*rightFill)(int, int, int),
	float depth, float edgeDepthThreshold,
	const char* leftGlyphNear, const char* leftGlyphFar,
	const char* rightGlyphNear, const char* rightGlyphFar,
	Craft::Color leftColor, Craft::Color rightColor,
	int sortingOrder)
{
	const int leftX = slice.centerX - slice.halfWidth;
	const int rightX = slice.centerX + slice.halfWidth;
	(this->*leftFill)(y, 0, leftX - 1);
	(this->*rightFill)(y, rightX + 1, screenWidth - 1);
	const char* leftGlyph = depth > edgeDepthThreshold ? leftGlyphFar : leftGlyphNear;
	const char* rightGlyph = depth > edgeDepthThreshold ? rightGlyphFar : rightGlyphNear;
	Craft::Renderer::Get().Submit(leftGlyph, Craft::Vector2(leftX, y),
		leftColor, sortingOrder);
	Craft::Renderer::Get().Submit(rightGlyph, Craft::Vector2(rightX, y),
		rightColor, sortingOrder);
}

void PolarLevel::DrawCanyonWall(int y, int startX, int endX)
{
	DrawTerrainFill(y, startX, endX, Craft::Color::Cyan,
		[](int px, int py, float) { return (px + py) % 2 == 0; },
		[](int px, int py) { return (px + py) % 6 == 0 ? "#" : "."; });
}

void PolarLevel::DrawSnowSurface(int y, int startX, int endX)
{
	DrawTerrainFill(y, startX, endX, Craft::Color::BrightWhite,
		[](int px, int py, float) { return (px + py * 2) % 5 == 0; },
		[](int px, int py) { return (px + py) % 10 == 0 ? "*" : "."; });
}

void PolarLevel::DrawOcean(int y, int startX, int endX)
{
	DrawTerrainFill(y, startX, endX, Craft::Color::Blue,
		[](int px, int py, float) { return (px + py) % 3 == 0; },
		[](int px, int py) { return (px + py) % 6 == 0 ? "~" : "-"; });
}

void PolarLevel::DrawBrokenIce(int y, int startX, int endX)
{
	DrawTerrainFill(y, startX, endX, Craft::Color::Cyan,
		[](int px, int py, float depth)
		{
			const int spacing = 17 - static_cast<int>(depth * 9.0f);
			return (px * 3 + py * 5) % spacing == 0;
		},
		[](int px, int py) { return (px + py) % 2 == 0 ? "/" : "\\"; });
}

void PolarLevel::DrawResearchBase(int y, int startX, int endX)
{
	DrawTerrainFill(y, startX, endX, Craft::Color::BrightWhite,
		[](int px, int py, float depth)
		{
			const int spacing = 10 - static_cast<int>(depth * 5.0f);
			return (px + py * 2) % spacing == 0;
		},
		[](int px, int py) { return (py % 4 == 0) ? "#" : ((px % 5 == 0) ? "|" : "_"); });
}

void PolarLevel::DrawTerrainFill(int y, int startX, int endX, Craft::Color color,
	const std::function<bool(int, int, float)>& shouldDraw,
	const std::function<const char*(int, int)>& glyphFor)
{
	startX = std::clamp(startX, 0, screenWidth - 1);
	endX = std::clamp(endX, 0, screenWidth - 1);
	const float depth = ScreenYToRoadDepth(y);
	for (int x = startX; x <= endX; ++x)
	{
		if (shouldDraw(x, y, depth))
		{
			Craft::Renderer::Get().Submit(glyphFor(x, y), Craft::Vector2(x, y),
				color, 0);
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
		<< "  ROAD: " << GetCurveDirection()
		<< "  STAR: " << collectedStarCount << " / " << RequiredStarCount
		<< "  SHOT: " << nonShotCount;
	Craft::Renderer::Get().Submit(hud.str(), Craft::Vector2(1, 0),
		Craft::Color::BrightWhite, 1000);
	Craft::Renderer::Get().Submit("ARROWS: Glide / Forward / Back   SPACE: Jump   ESC: Menu",
		Craft::Vector2(1, 1), Craft::Color::Cyan, 1000);
	if (speedNotificationTimer > 0.0f)
	{
		Craft::Renderer::Get().Submit(speedNotification,
			Craft::Vector2(screenWidth / 2
				- static_cast<int>(speedNotification.size()) / 2, 3),
			speedNotificationStage >= 3
				? Craft::Color::Yellow : Craft::Color::Cyan,
			1600);
	}
	if (nonShotCount > 0)
	{
		const std::string ready = "SHOT READY!";
		Craft::Renderer::Get().Submit(ready,
			Craft::Vector2(screenWidth / 2
				- static_cast<int>(ready.size()) / 2, 2),
			Craft::Color::Green, 1600);
	}
	if (coastEnemyWarningTimer > 0.0f)
	{
		const std::string warning = "ENEMY SHIP APPROACHING!";
		Craft::Renderer::Get().Submit(warning,
			Craft::Vector2(screenWidth / 2
				- static_cast<int>(warning.size()) / 2, 4),
			Craft::Color::Red, 1700);
	}
	if (IsPlaying() && IsOnNarrowIcePath())
	{
		constexpr float warningBoundary = 0.90f;
		const bool nearEdge = player
			&& std::abs(player->GetHorizontalPosition()) > warningBoundary;
		const std::string warning = nearEdge
			? "!!! EDGE WARNING - STEER BACK !!!"
			: "CAUTION: SLIPPERY NARROW ICE";
		Craft::Renderer::Get().Submit(warning,
			Craft::Vector2(screenWidth / 2
				- static_cast<int>(warning.size()) / 2, 2),
			nearEdge ? Craft::Color::Red : Craft::Color::Yellow, 1500);
	}

	if (state == State::Crashed)
	{
		const std::string message = hitByEnemyBullet
			? "CRASH: HIT BY ENEMY BULLET   R : Retry"
			: (fellThroughBrokenBridge
			? "CRASH: FELL THROUGH BROKEN BRIDGE   R : Retry"
			: (fellFromNarrowIcePath
				? "CRASH: FELL OFF NARROW ICE PATH   R : Retry"
				: std::string("CRASH: ") + GetObstacleName(crashedObstacleType)
					+ "   R : Retry"));
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

void PolarLevel::DrawPauseMenu()
{
	const bool canResume = stateBeforePause == State::Playing;
	const char* items[] = { canResume ? "RESUME" : "BACK", "RETRY", "QUIT" };
	const std::string title = canResume ? "PAUSED" : "MENU";
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
	const std::string footer = canResume
		? "UP/DOWN + ENTER   ESC : RESUME"
		: "UP/DOWN + ENTER   ESC : BACK";
	Craft::Renderer::Get().Submit(footer,
		Craft::Vector2(screenWidth / 2 - 16, centerY + 4),
		Craft::Color::BrightWhite, 3000);
}

void PolarLevel::Draw()
{
	DrawSkyAndHorizon();
	DrawPerspectiveRoad();
	DrawNarrowPathWarningSign();
	Level::Draw();
	DrawHud();
	if (state == State::PauseMenu)
	{
		DrawPauseMenu();
	}
}
