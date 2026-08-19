#include "PolarLevel.h"

#include <Actor/PolarObstacle.h>
#include <Actor/Player.h>
#include <Actor/PlayerBullet.h>
#include <Actor/PolarStar.h>
#include <Actor/Enemy.h>
#include <Actor/EnemyBullet.h>
#include <Engine/Engine.h>
#include <Input/Input.h>
#include <Math/Color.h>
#include <Render/Renderer.h>
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <utility>

namespace
{
	// 장애물 생성과 Enemy 구간 판정이 서로 다른 레인/거리 값을 쓰지 않도록
	// 공유합니다.
	constexpr float kLaneOffsets[] = { -0.55f, 0.0f, 0.55f };
	// Enemy 전투는 반드시 Coast 지형 안에서 시작하고 끝납니다. 경고와 접근
	// 시간까지 확보할 수 있도록 일반 장애물 구간보다 충분히 길게 잡습니다.
	constexpr float kEnemyZoneStarts[] =
	{
		1120.0f, 1630.0f, 2580.0f, 3680.0f, 4480.0f,
		6120.0f, 6630.0f, 7580.0f, 8680.0f, 9480.0f,
		11120.0f, 11630.0f, 12580.0f, 13680.0f, 14480.0f
	};
	constexpr float kEnemyZoneEnds[] =
	{
		1280.0f, 1790.0f, 2800.0f, 3920.0f, 4770.0f,
		6280.0f, 6790.0f, 7800.0f, 8920.0f, 9770.0f,
		11280.0f, 11790.0f, 12800.0f, 13920.0f, 14770.0f
	};
	constexpr int kEnemyZoneCount =
		static_cast<int>(sizeof(kEnemyZoneStarts) / sizeof(kEnemyZoneStarts[0]));

	bool IsInsideEnemyZone(float distance)
	{
		for (int zoneIndex = 0; zoneIndex < kEnemyZoneCount; ++zoneIndex)
		{
			if (distance >= kEnemyZoneStarts[zoneIndex]
				&& distance <= kEnemyZoneEnds[zoneIndex])
			{
				return true;
			}
		}
		return false;
	}

	void AppendPlaytestLog(const std::string& line)
	{
#ifdef _DEBUG
		std::ofstream log("PlaytestLog.log", std::ios::app);
		if (log.is_open())
		{
			log << line << "\n";
		}
#endif
	}
}

void PolarLevel::LogEvent(const std::string& message) const
{
	std::ostringstream line;
	line << "[" << std::fixed << std::setprecision(1) << traveledDistance
		<< "m] " << message;
	AppendPlaytestLog(line.str());
}

void PolarLevel::OnInitialized()
{
	Level::OnInitialized();
	traveledDistance = std::clamp(nextStartDistance, 0.0f, courseDistance);
	screenWidth = Craft::Engine::Get().GetWidth();
	screenHeight = Craft::Engine::Get().GetHeight();
	horizonY = 7;
	playerScreenY = screenHeight - 6;
	player = SpawnActor<Player>();
	BuildRoadCourse();
	BuildTestCourse();
	LogEvent("=== 새 런 시작 ===");
	LogObstacleLayout();
}

void PolarLevel::BuildRoadCourse()
{
	roadSlices =
	{
		// 넓은 시작 설원
		{ 0.0f,    0.00f, 1.25f, TerrainType::Snowfield },
		{ 40.0f,   0.00f, 1.25f, TerrainType::Snowfield },

		// 왼쪽 해안
		{ 80.0f,  -0.55f, 0.90f, TerrainType::Coast },
		{ 150.0f, -0.55f, 0.82f, TerrainType::Coast },

		// 오른쪽으로 꺾이는 협곡
		{ 180.0f,  0.10f, 0.65f, TerrainType::Canyon },
		{ 230.0f,  0.60f, 0.55f, TerrainType::Canyon },

		// 반대 방향 깨진 빙판
		{ 260.0f,  0.00f, 0.95f, TerrainType::BrokenIce },
		{ 300.0f, -0.60f, 0.88f, TerrainType::BrokenIce },
		{ 360.0f, -0.60f, 0.92f, TerrainType::BrokenIce },

		// 연구기지 S자 구간
		{ 400.0f,  0.00f, 1.05f, TerrainType::ResearchBase },
		{ 440.0f,  0.60f, 0.92f, TerrainType::ResearchBase },
		{ 490.0f, -0.60f, 0.88f, TerrainType::ResearchBase },
		{ 540.0f,  0.55f, 0.92f, TerrainType::ResearchBase },

		// 넓은 설원
		// 다섯 번째 별 이후 밀렵선이 등장하는 두 번째 해안
		{ 580.0f,  0.00f, 1.30f, TerrainType::Coast },
		{ 650.0f, -0.45f, 1.15f, TerrainType::Coast },
		{ 710.0f,  0.45f, 1.10f, TerrainType::Coast },

		// 좁은 얼음다리
		{ 750.0f,  0.00f, 0.55f, TerrainType::NarrowIcePath },
		{ 810.0f, -0.25f, 0.40f, TerrainType::NarrowIcePath },
		{ 870.0f,  0.25f, 0.40f, TerrainType::NarrowIcePath },

		// 북극 도착 구간
		{ 900.0f,  0.00f, 1.10f, TerrainType::Snowfield },
		{ 950.0f, -0.35f, 1.20f, TerrainType::Snowfield },
		{ 1000.0f, 0.00f, 1.35f, TerrainType::Snowfield },

		// 후반 진입: 넓은 설원에서 속도에 적응합니다.
		{ 1080.0f, 0.35f, 1.25f, TerrainType::Snowfield },

		// 두 번째 밀렵선이 등장하는 해안 구간입니다.
		{ 1120.0f, 0.10f, 1.10f, TerrainType::Coast },
		{ 1190.0f, -0.50f, 0.95f, TerrainType::Coast },
		{ 1260.0f, 0.45f, 1.05f, TerrainType::Coast },
		// Enemy 구간(kEnemyZoneEnds=1260) 이후에도 바다 배경이 한동안
		// 유지되어야 격추할 시간이 충분합니다.
		{ 1300.0f, 0.25f, 1.00f, TerrainType::Coast },

		// 빠른 연속 조향을 요구하는 협곡과 연구기지입니다.
		{ 1350.0f, 0.55f, 0.62f, TerrainType::Canyon },
		{ 1420.0f, -0.55f, 0.58f, TerrainType::Canyon },
		{ 1460.0f, 0.45f, 0.92f, TerrainType::ResearchBase },
		{ 1540.0f, -0.45f, 0.88f, TerrainType::ResearchBase },

		// 세 번째 밀렵선이 등장하는 마지막 해안 구간입니다.
		{ 1630.0f, 0.00f, 1.15f, TerrainType::Coast },
		{ 1700.0f, 0.50f, 1.00f, TerrainType::Coast },
		{ 1770.0f, -0.45f, 0.95f, TerrainType::Coast },
		// Enemy 구간(kEnemyZoneEnds=1770) 이후에도 바다 배경을 조금 더 유지합니다.
		{ 1800.0f, -0.20f, 0.90f, TerrainType::Coast },

		// 결승 전에는 다리 점프와 좁은 길 조향을 다시 확인합니다.
		{ 1825.0f, 0.00f, 0.85f, TerrainType::BrokenIce },
		{ 1870.0f, 0.30f, 0.48f, TerrainType::NarrowIcePath },
		{ 1930.0f, -0.30f, 0.52f, TerrainType::NarrowIcePath },
		{ 1970.0f, 0.00f, 1.20f, TerrainType::Snowfield },
		{ 2000.0f, 0.00f, 1.35f, TerrainType::Snowfield },

		// 2000~2500m: 연구기지를 지나 협곡으로 들어가는 장거리 구간
		{ 2080.0f, 0.35f, 1.05f, TerrainType::ResearchBase },
		{ 2180.0f, -0.50f, 0.92f, TerrainType::ResearchBase },
		{ 2300.0f, 0.45f, 0.72f, TerrainType::Canyon },
		{ 2440.0f, -0.55f, 0.60f, TerrainType::Canyon },
		{ 2520.0f, 0.00f, 1.20f, TerrainType::Coast },

		// 세 번째 Enemy 해안. 전투 뒤에도 바다를 남겨 전환을 자연스럽게 합니다.
		{ 2580.0f, -0.30f, 1.12f, TerrainType::Coast },
		{ 2700.0f, 0.45f, 1.00f, TerrainType::Coast },
		{ 2800.0f, -0.35f, 0.96f, TerrainType::Coast },
		{ 2860.0f, 0.00f, 1.05f, TerrainType::Coast },

		// 2900~3550m: 깨진 빙판과 좁은 길 뒤에 다시 넓은 설원
		{ 2920.0f, 0.35f, 0.92f, TerrainType::BrokenIce },
		{ 3040.0f, -0.30f, 0.52f, TerrainType::NarrowIcePath },
		{ 3160.0f, 0.30f, 0.46f, TerrainType::NarrowIcePath },
		{ 3260.0f, 0.00f, 1.25f, TerrainType::Snowfield },
		{ 3400.0f, -0.55f, 1.05f, TerrainType::ResearchBase },
		{ 3540.0f, 0.45f, 0.92f, TerrainType::ResearchBase },

		// 네 번째 Enemy 해안
		{ 3620.0f, 0.00f, 1.22f, TerrainType::Coast },
		{ 3680.0f, 0.40f, 1.10f, TerrainType::Coast },
		{ 3800.0f, -0.50f, 0.96f, TerrainType::Coast },
		{ 3920.0f, 0.35f, 1.02f, TerrainType::Coast },
		{ 3980.0f, 0.00f, 1.08f, TerrainType::Coast },

		// 4050~4400m: 마지막 전투 전 고난도 지형
		{ 4050.0f, 0.50f, 0.62f, TerrainType::Canyon },
		{ 4170.0f, -0.45f, 0.88f, TerrainType::BrokenIce },
		{ 4290.0f, 0.28f, 0.48f, TerrainType::NarrowIcePath },
		{ 4400.0f, 0.00f, 1.18f, TerrainType::Snowfield },

		// 다섯 번째이자 가장 긴 Enemy 해안
		{ 4450.0f, -0.25f, 1.20f, TerrainType::Coast },
		{ 4480.0f, 0.35f, 1.12f, TerrainType::Coast },
		{ 4620.0f, -0.50f, 0.98f, TerrainType::Coast },
		{ 4770.0f, 0.40f, 1.04f, TerrainType::Coast },
		{ 4830.0f, 0.00f, 1.12f, TerrainType::Coast },

		// 결승 직전에는 넓은 설원으로 시야를 열어 줍니다.
		{ 4890.0f, -0.25f, 1.25f, TerrainType::Snowfield },
		{ 5000.0f, 0.00f, 1.35f, TerrainType::Snowfield }
	};

	// 첫 5000m에서 검증한 지형 흐름을 두 차례 더 이어 붙여 약 10분
	// 코스를 구성합니다. 두 번째 구간은 좌우를 반전해 반복감을 줄입니다.
	const std::vector<RoadSlice> baseCourse = roadSlices;
	for (int repeatIndex = 1; repeatIndex <= 2; ++repeatIndex)
	{
		const float distanceOffset = 5000.0f * repeatIndex;
		const float direction = repeatIndex % 2 == 1 ? -1.0f : 1.0f;
		for (const RoadSlice& baseSlice : baseCourse)
		{
			if (baseSlice.distance <= 0.0f)
			{
				continue;
			}
			roadSlices.push_back(
				{ baseSlice.distance + distanceOffset,
				  baseSlice.centerOffset * direction,
				  baseSlice.width, baseSlice.terrain });
		}
	}
}

void PolarLevel::SetNextStartDistance(float distance)
{
	nextStartDistance = (std::max)(0.0f, distance);
}

void PolarLevel::BuildTestCourse()
{
	// Widths use the same normalized road coordinates as player movement and
	// match the near-camera four-column spike and eight-column wall sprites.
	constexpr float spikeHalfWidth = 0.055f;
	constexpr float wallHalfWidth = 0.11f;
	constexpr float puddleHalfWidth = 0.16f;
	const auto add = [this](float horizontalPosition, float distance,
		ObstacleType type, float horizontalHalfWidth)
	{
		obstacles.emplace_back(SpawnActor<PolarObstacle>(
			horizontalPosition, distance - traveledDistance,
			type, horizontalHalfWidth));
	};
	const auto spike = [&add](float x, float distance)
	{
		add(x, distance, ObstacleType::LowSpike, spikeHalfWidth);
	};
	const auto wall = [&add](float x, float distance)
	{
		add(x, distance, ObstacleType::IceWall, wallHalfWidth);
	};
	const auto puddle = [&add](float x, float distance)
	{
		add(x, distance, ObstacleType::Puddle, puddleHalfWidth);
	};
	const auto brokenBridge = [&add](float distance)
	{
		add(0.0f, distance, ObstacleType::BrokenBridge, 1.0f);
	};

	// Keep the first obstacle fixed so every run teaches jumping before random
	// decisions begin.
	spike(0.0f, 55.0f);

	static unsigned int retrySequence = 0;
	std::random_device randomDevice;
	const unsigned long long clockSeed = static_cast<unsigned long long>(
		std::chrono::high_resolution_clock::now().time_since_epoch().count());
	std::seed_seq seed
	{
		randomDevice(), randomDevice(), retrySequence++,
		static_cast<unsigned int>(clockSeed),
		static_cast<unsigned int>(clockSeed >> 32)
	};
	std::mt19937 random(seed);

	// 해안 구간마다 바다가 왼쪽/오른쪽 중 어느 쪽에 그려질지 미리 정해둡니다.
	// DrawCoastRow와 Enemy 스폰 위치가 이 값을 함께 참조해서, 배가 항상
	// 배경의 바다 쪽에서만 나타나도록 맞춥니다.
	coastOceanOnRight.assign(kEnemyZoneCount, false);
	std::uniform_int_distribution<int> oceanSideChoice(0, 1);
	for (int zoneIndex = 0; zoneIndex < kEnemyZoneCount; ++zoneIndex)
	{
		coastOceanOnRight[zoneIndex] = oceanSideChoice(random) == 1;
	}

	const float(&lanes)[3] = kLaneOffsets;
	std::uniform_int_distribution<int> laneChoice(0, 2);
	std::uniform_int_distribution<int> typeChoice(0, 2);
	const auto randomSpacing = [&random](float minimum, float maximum)
	{
		return std::uniform_real_distribution<float>(minimum, maximum)(random);
	};
	const auto randomObstacle = [&]()
	{
		const int typeIndex = typeChoice(random);
		const ObstacleType type = typeIndex == 0
			? ObstacleType::LowSpike
			: (typeIndex == 1 ? ObstacleType::IceWall : ObstacleType::Puddle);
		return std::pair<float, ObstacleType>(
			lanes[laneChoice(random)], type);
	};
	const auto spawnObstacle = [&](float lane, float distance, ObstacleType type)
	{
		if (type == ObstacleType::LowSpike)
		{
			spike(lane, distance);
		}
		else if (type == ObstacleType::Puddle)
		{
			puddle(lane, distance);
		}
		else
		{
			wall(lane, distance);
		}
	};
	const auto doubleWall = [&](int safeLaneIndex, float distance)
	{
		for (int laneIndex = 0; laneIndex < 3; ++laneIndex)
		{
			if (laneIndex != safeLaneIndex)
			{
				wall(lanes[laneIndex], distance);
			}
		}
	};
	std::uniform_real_distribution<float> patternJitter(-10.0f, 10.0f);
	const float doubleWallDistances[] =
	{
		400.0f + patternJitter(random),
		650.0f + patternJitter(random),
		930.0f + patternJitter(random)
	};
	const float comboStarts[] =
	{
		250.0f + patternJitter(random),
		500.0f + patternJitter(random),
		700.0f + patternJitter(random)
	};
	const auto isInsideAuthoredPattern =
		[&doubleWallDistances, &comboStarts](float distance)
	{
		constexpr float safeSpacing = 35.0f;
		for (float patternDistance : doubleWallDistances)
		{
			if (std::abs(distance - patternDistance) < safeSpacing)
			{
				return true;
			}
		}
		for (float patternStart : comboStarts)
		{
			if (distance >= patternStart - 25.0f
				&& distance <= patternStart + 55.0f)
			{
				return true;
			}
		}
		return false;
	};

	// One obstacle per distance guarantees at least two lateral escape routes.
	// Spacing becomes shorter as the course progresses, so later sections ask
	// for more frequent decisions without creating impossible lane walls.
	for (float distance = 125.0f; distance < 730.0f;)
	{
		if (!isInsideAuthoredPattern(distance))
		{
			const auto [lane, type] = randomObstacle();
			spawnObstacle(lane, distance, type);
		}

		if (distance < 300.0f)
		{
			distance += randomSpacing(65.0f, 80.0f);
		}
		else if (distance < 550.0f)
		{
			distance += randomSpacing(50.0f, 65.0f);
		}
		else
		{
			distance += randomSpacing(35.0f, 50.0f);
		}
	}

	// Retry changes each safe lane and shifts the pattern slightly while still
	// guaranteeing that every double wall leaves one route open.
	for (float patternDistance : doubleWallDistances)
	{
		doubleWall(laneChoice(random), patternDistance);
	}

	// Short authored combinations behave like input sentences: steer first,
	// then immediately decide whether to steer again or jump.
	const ObstacleType firstTypes[] =
	{
		ObstacleType::IceWall, ObstacleType::Puddle, ObstacleType::IceWall
	};
	const ObstacleType secondTypes[] =
	{
		ObstacleType::LowSpike, ObstacleType::IceWall, ObstacleType::LowSpike
	};
	std::uniform_int_distribution<int> turnChoice(1, 2);
	for (int patternIndex = 0; patternIndex < 3; ++patternIndex)
	{
		const int firstLane = laneChoice(random);
		const int secondLane = (firstLane + turnChoice(random)) % 3;
		spawnObstacle(lanes[firstLane], comboStarts[patternIndex],
			firstTypes[patternIndex]);
		spawnObstacle(lanes[secondLane], comboStarts[patternIndex]
			+ randomSpacing(24.0f, 30.0f), secondTypes[patternIndex]);
	}

	// Broken bridges stay inside the narrow-path section, but their exact
	// positions vary while retaining enough recovery distance between jumps.
	std::uniform_real_distribution<float> bridgeJitter(-8.0f, 8.0f);
	brokenBridge(790.0f + bridgeJitter(random));
	brokenBridge(850.0f + bridgeJitter(random));

	// Increase the final snowfield density after the bridge section. Each row
	// still contains only one obstacle, leaving a readable escape route.
	for (float distance = 905.0f; distance < 980.0f;
		distance += randomSpacing(30.0f, 45.0f))
	{
		if (!isInsideAuthoredPattern(distance))
		{
			const auto [lane, type] = randomObstacle();
			spawnObstacle(lane, distance, type);
		}
	}

	// 고정 점프 구간 주변에는 랜덤 장애물을 만들지 않습니다. 그렇지 않으면
	// BrokenBridge와 IceWall이 겹쳐 통과 불가능한 조합이 생길 수 있습니다.
	const float lateAuthoredHazards[] =
	{
		1835.0f, 1905.0f, 1960.0f, 2990.0f, 3120.0f, 4210.0f, 4350.0f
	};
	const auto isNearLateAuthoredHazard = [&lateAuthoredHazards](float distance)
	{
		constexpr float reservedSpacing = 32.0f;
		for (float hazardDistance : lateAuthoredHazards)
		{
			if (std::abs(distance - hazardDistance) < reservedSpacing)
			{
				return true;
			}
		}
		return false;
	};

	// 후반부는 속도가 높으므로 장애물 간격을 급격히 줄이지 않습니다.
	// 한 거리에는 하나만 배치해 항상 좌우 회피 공간을 남깁니다.
	for (float distance = 1040.0f; distance < 14930.0f;)
	{
		const bool isEnemyCombatSection = IsInsideEnemyZone(distance);
		if (!isEnemyCombatSection && !isNearLateAuthoredHazard(distance))
		{
			const auto [lane, type] = randomObstacle();
			spawnObstacle(lane, distance, type);
		}
		if (distance < 2000.0f)
		{
			distance += distance < 1450.0f
				? randomSpacing(42.0f, 58.0f)
				: randomSpacing(34.0f, 48.0f);
		}
		else if (distance < 3500.0f)
		{
			distance += randomSpacing(30.0f, 43.0f);
		}
		else if (distance < 10000.0f)
		{
			distance += randomSpacing(26.0f, 38.0f);
		}
		else
		{
			// 최고 속도 구간에서도 연속 회피가 불가능해지지 않도록
			// 최소 간격은 더 줄이지 않습니다.
			distance += randomSpacing(26.0f, 36.0f);
		}
	}

	// 결승 전에는 점프, 안전 레인 선택, 점프 순서로 마무리합니다.
	// 1870~1930m는 도로 폭이 가장 좁은 NarrowIcePath 구간이라 doubleWall(2/3 차단)
	// 대신 단일 IceWall만 배치해 좁은 도로와 겹쳐 난이도가 튀지 않게 합니다.
	brokenBridge(1835.0f + bridgeJitter(random));
	wall(lanes[laneChoice(random)], 1905.0f + patternJitter(random));
	spike(lanes[laneChoice(random)], 1960.0f + patternJitter(random));
	brokenBridge(2990.0f + bridgeJitter(random));
	brokenBridge(3120.0f + bridgeJitter(random));
	brokenBridge(4210.0f + bridgeJitter(random));
	wall(lanes[laneChoice(random)], 4350.0f + patternJitter(random));

	// Obstacles are finalized before collectibles. Each star then selects a lane
	// with enough same-lane clearance; obstacles in other lanes remain allowed.
	std::vector<float> starDistances =
	{
		90.0f, 195.0f, 315.0f, 450.0f, 575.0f, 750.0f, 890.0f,
		1060.0f, 1160.0f, 1285.0f, 1415.0f, 1570.0f, 1660.0f,
		1785.0f, 1880.0f, 1975.0f,
		2070.0f, 2200.0f, 2340.0f, 2490.0f, 2600.0f, 2720.0f,
		2840.0f, 2970.0f, 3090.0f, 3220.0f, 3350.0f, 3490.0f,
		3630.0f, 3740.0f, 3860.0f, 3990.0f, 4120.0f, 4250.0f,
		4380.0f, 4500.0f, 4620.0f, 4740.0f, 4860.0f, 4960.0f
	};
	// 5000m 이후에도 별을 일정하게 공급하되 간격을 조금 흔들어
	// 같은 위치가 반복되는 느낌을 줄입니다.
	for (float distance = 5100.0f; distance < 14950.0f;
		distance += randomSpacing(105.0f, 145.0f))
	{
		starDistances.emplace_back(distance);
	}
	constexpr float starObstacleSpacing = 20.0f;
	for (float starDistance : starDistances)
	{
		std::vector<int> safeLaneIndices;
		for (int laneIndex = 0; laneIndex < 3; ++laneIndex)
		{
			bool isSafe = true;
			for (const std::shared_ptr<PolarObstacle>& obstacle : obstacles)
			{
				const bool blocksEveryLane =
					obstacle->GetObstacleType() == ObstacleType::BrokenBridge;
				const bool sameLane = std::abs(obstacle->GetHorizontalPosition()
					- lanes[laneIndex]) < 0.20f;
				if ((blocksEveryLane || sameLane)
					&& std::abs(obstacle->GetDistance() - starDistance)
						< starObstacleSpacing)
				{
					isSafe = false;
					break;
				}
			}
			if (isSafe)
			{
				safeLaneIndices.emplace_back(laneIndex);
			}
		}
		if (!safeLaneIndices.empty())
		{
			std::uniform_int_distribution<int> safeLaneChoice(
				0, static_cast<int>(safeLaneIndices.size()) - 1);
			const int laneIndex = safeLaneIndices[safeLaneChoice(random)];
			stars.emplace_back(SpawnActor<PolarStar>(
				lanes[laneIndex], starDistance - traveledDistance));
		}
	}
}

void PolarLevel::LogObstacleLayout() const
{
	LogEvent("=== 장애물 배치 (" + std::to_string(obstacles.size()) + "개) ===");
	for (const std::shared_ptr<PolarObstacle>& obstacle : obstacles)
	{
		std::ostringstream entry;
		entry << "OBSTACLE " << GetObstacleName(obstacle->GetObstacleType())
			<< " lane=" << std::fixed << std::setprecision(2)
			<< obstacle->GetHorizontalPosition()
			<< " dist=" << std::setprecision(1) << obstacle->GetDistance();
		LogEvent(entry.str());
	}

	// Enemy 전용 해안 구간에는 일반 장애물이 없어야 합니다.
	for (const std::shared_ptr<PolarObstacle>& obstacle : obstacles)
	{
		const float distance = obstacle->GetDistance();
		for (int zoneIndex = 0; zoneIndex < kEnemyZoneCount; ++zoneIndex)
		{
			if (distance >= kEnemyZoneStarts[zoneIndex]
				&& distance <= kEnemyZoneEnds[zoneIndex])
			{
				LogEvent("WARNING: Enemy 구간(zone " + std::to_string(zoneIndex)
					+ ") 안에 " + GetObstacleName(obstacle->GetObstacleType())
					+ " 장애물 발견 dist=" + std::to_string(distance));
			}
		}
	}

	// IceWall은 점프로 피할 수 없으므로 세 레인을 동시에 막으면 완주가
	// 불가능합니다. 서로 다른 생성 구간(연속 랜덤/이중벽/조합 패턴)이 겹칠 때만
	// 발생할 수 있는 조합이라 실행마다 자동으로 검사합니다.
	constexpr float hazardWindow = 12.0f;
	constexpr float laneMatchTolerance = 0.25f;
	std::vector<float> warnedDistances;
	for (const std::shared_ptr<PolarObstacle>& obstacle : obstacles)
	{
		if (obstacle->GetObstacleType() != ObstacleType::IceWall)
		{
			continue;
		}
		const float centerDistance = obstacle->GetDistance();
		bool alreadyWarned = false;
		for (float warned : warnedDistances)
		{
			if (std::abs(warned - centerDistance) < hazardWindow)
			{
				alreadyWarned = true;
				break;
			}
		}
		if (alreadyWarned)
		{
			continue;
		}

		bool laneBlocked[3] = { false, false, false };
		for (const std::shared_ptr<PolarObstacle>& other : obstacles)
		{
			if (other->GetObstacleType() != ObstacleType::IceWall
				|| std::abs(other->GetDistance() - centerDistance) > hazardWindow)
			{
				continue;
			}
			for (int laneIndex = 0; laneIndex < 3; ++laneIndex)
			{
				if (std::abs(other->GetHorizontalPosition()
					- kLaneOffsets[laneIndex]) < laneMatchTolerance)
				{
					laneBlocked[laneIndex] = true;
				}
			}
		}
		if (laneBlocked[0] && laneBlocked[1] && laneBlocked[2])
		{
			LogEvent("WARNING: IceWall이 세 레인을 모두 막는 조합 발견 dist~="
				+ std::to_string(centerDistance));
			warnedDistances.push_back(centerDistance);
		}
	}
	LogEvent("=== 장애물 배치 끝 ===");
}

void PolarLevel::Tick(float deltaTime)
{
	HandleMenuInput();
	if (!IsPlaying())
	{
		return;
	}

	UpdateCurve(deltaTime);
	UpdateRunSpeed(deltaTime);
	Level::Tick(deltaTime);
	HandlePlayerFire();
	traveledDistance += runSpeed * deltaTime;
	while (nextBalanceLogDistance <= courseDistance
		&& traveledDistance >= nextBalanceLogDistance)
	{
		std::ostringstream entry;
		entry << "CHECKPOINT speed=" << std::fixed << std::setprecision(1)
			<< runSpeed;
		LogEvent(entry.str());
		nextBalanceLogDistance += 100.0f;
	}
	UpdateCoastEnemy(deltaTime);
	UpdateSpeedNotification(deltaTime);
	CheckTerrainHazards(deltaTime);
	CheckObstacleCollisions();
	CheckStarCollections();
	CheckEnemyBulletCollisions();
	if (traveledDistance >= courseDistance && IsPlaying())
	{
		state = State::Goal;
		LogEvent("GOAL 도달 (speed=" + std::to_string(runSpeed) + ")");
	}
}

void PolarLevel::UpdateCoastEnemy(float deltaTime)
{
	const TerrainType terrain = GetRoadProfile(traveledDistance).terrain;

	// Do not carry a shooting encounter into a narrow, slippery section.
	// The enemy retreats while the narrow path is still far enough away to be
	// read normally, so the authored road is never hidden and then swapped back.
	constexpr float enemyRetreatBeforeSlipDistance = 120.0f;
	if (coastEnemy && !coastEnemy->HasExpired()
		&& IsNarrowIcePathAhead(enemyRetreatBeforeSlipDistance))
	{
		LogEvent("Enemy retreat before narrow ice path");
		coastEnemy->Destroy();
		for (const std::shared_ptr<EnemyBullet>& bullet : enemyBullets)
		{
			if (bullet && !bullet->HasExpired())
			{
				bullet->Destroy();
			}
		}

		// This is an intentional early retreat while the current authored terrain
		// is still Coast. Do not start the post-kill coast override, otherwise the
		// upcoming narrow path would be concealed and appear suddenly later.
		enemyFireTimer = 0.0f;
		enemyFirePatternIndex = 0;
	}

	// SHOT이 없는 채 구간을 통과했다면 다음 해안 구간을 기다립니다.
	while (nextCoastEnemyZoneIndex < kEnemyZoneCount
		&& traveledDistance > kEnemyZoneEnds[nextCoastEnemyZoneIndex])
	{
		++nextCoastEnemyZoneIndex;
		coastEnemyWarningTimer = 0.0f;
	}

	// 격추하지 못한 채 플레이어가 해안(바다 배경)을 벗어나면 Enemy를
	// 자동으로 소멸시킵니다. 그대로 두면 바다가 없는 지형에서도 배가
	// 계속 쫓아오는 것처럼 보입니다.
	// 한번 접근한 Enemy는 지형 전환으로 제거하지 않고 격추될 때까지 추격합니다.
	if (coastEnemy && !coastEnemy->HasExpired()
		&& terrain != TerrainType::Coast)
	{
		coastEnemy->Destroy();
		// 이미 발사된 탄환도 정리합니다. 그대로 두면 화면을 다 건너가기 전까지
		// 계속 날아다니다가, 지형이 한참 바뀐 뒤에도 플레이어를 맞힐 수 있습니다.
		for (const std::shared_ptr<EnemyBullet>& bullet : enemyBullets)
		{
			if (bullet && !bullet->HasExpired())
			{
				bullet->Destroy();
			}
		}
	}

	const bool hasActiveEnemy = coastEnemy && !coastEnemy->HasExpired();
	if (coastEnemyWasActive && !hasActiveEnemy)
	{
		LogEvent("Enemy 제거됨 (격추 또는 소멸)");
		// 새 지형이 지평선부터 내려오도록 전환 시간을 시작합니다.
		enemyFireTimer = 0.0f;
		enemyFirePatternIndex = 0;
	}

	const bool isInsideNextEnemyZone =
		nextCoastEnemyZoneIndex < kEnemyZoneCount
		&& traveledDistance >= kEnemyZoneStarts[nextCoastEnemyZoneIndex]
		&& traveledDistance <= kEnemyZoneEnds[nextCoastEnemyZoneIndex];

	constexpr float enemySpawnClearanceFromSlip = 220.0f;
	const bool hasSafeCombatSpace =
		!IsNarrowIcePathAhead(enemySpawnClearanceFromSlip);
	if (!hasActiveEnemy && isInsideNextEnemyZone
		&& terrain == TerrainType::Coast && nonShotCount > 0
		&& hasSafeCombatSpace)
	{
		if (coastEnemyWarningTimer <= 0.0f)
		{
			Craft::Engine::Get().PlayOneShot("alarm.wav");
			LogEvent("Enemy 경고 시작 zone="
				+ std::to_string(nextCoastEnemyZoneIndex));
		}
		coastEnemyWarningTimer += deltaTime;
		constexpr float warningDuration = 2.0f;
		if (coastEnemyWarningTimer >= warningDuration)
		{
			constexpr float initialDistance = 95.0f;
			// 배경(DrawCoastRow)에 바다가 그려지는 쪽과 맞춰서 스폰합니다.
			const EnemySide spawnSide = coastOceanOnRight[nextCoastEnemyZoneIndex]
				? EnemySide::Right : EnemySide::Left;
			coastEnemy = SpawnActor<Enemy>(initialDistance, spawnSide);
			coastEnemyScreenX = spawnSide == EnemySide::Left
				? screenWidth * 0.18f : screenWidth * 0.82f;
			enemyFireTimer = 0.0f;
			enemyFirePatternIndex = 0;
			LogEvent("Enemy 출현 zone="
				+ std::to_string(nextCoastEnemyZoneIndex)
				+ " side=" + (spawnSide == EnemySide::Left ? "Left" : "Right"));
			++nextCoastEnemyZoneIndex;
			coastEnemyWarningTimer = 0.0f;
		}
	}
	else if (!hasActiveEnemy)
	{
		coastEnemyWarningTimer = 0.0f;
	}

	if (!coastEnemy || coastEnemy->HasExpired())
	{
		return;
	}

	// 탄환이 생성되는 펭귄 머리 줄에 Enemy가 도달한 뒤 그 줄을 추적합니다.
	const int chaseScreenY = GetPlayerScreenY() - 1;
	const float chaseDistance = ScreenYToDistance(chaseScreenY);
	coastEnemy->Advance(runSpeed * deltaTime, chaseDistance);
	if (coastEnemy->HasExpired())
	{
		return;
	}

	const int screenY = DistanceToScreenY(coastEnemy->GetDistance());
	const int roadCenterX = GetRoadCenterX(screenY);
	const int roadHalfWidth = GetRoadHalfWidth(screenY);
	// 배의 총구와 선체가 도로를 침범하지 않도록 충분한 간격을 둡니다.
	constexpr int coastMargin = 8;

	const int playerScreenX = GetRoadScreenX(
		player->GetHorizontalPosition(), GetPlayerScreenY());
	const bool coastEnemyOnRight = coastEnemy->GetSide() == EnemySide::Right;
	const int desiredEnemyX = coastEnemyOnRight
		? (std::max)(playerScreenX + 25,
			roadCenterX + roadHalfWidth + coastEnemy->GetWidth() + coastMargin)
		: (std::min)(playerScreenX - 25,
			roadCenterX - roadHalfWidth - coastEnemy->GetWidth() - coastMargin);

	if (coastEnemy->GetState() == EnemyState::ClosingSide)
	{
		constexpr float sideApproachSpeed = 18.0f;
		coastEnemyScreenX += coastEnemyOnRight
			? -sideApproachSpeed * deltaTime : sideApproachSpeed * deltaTime;
		const bool reachedDesired = coastEnemyOnRight
			? coastEnemyScreenX <= desiredEnemyX
			: coastEnemyScreenX >= desiredEnemyX;
		if (reachedDesired)
		{
			coastEnemyScreenX = static_cast<float>(desiredEnemyX);
			coastEnemy->BeginChasing();
			LogEvent("Enemy Chasing 진입");
		}
	}
	else if (coastEnemy->GetState() == EnemyState::Chasing)
	{
		const float blend = (std::min)(deltaTime * 5.0f, 1.0f);
		coastEnemyScreenX +=
			(desiredEnemyX - coastEnemyScreenX) * blend;
	}

	const int screenX = static_cast<int>(coastEnemyScreenX);
	// screenX는 밀렵꾼 중앙이며 Actor 충돌 위치는 7칸 몸통의 왼쪽 끝입니다.
	coastEnemy->SetPosition(Craft::Vector2(screenX - 3, screenY));

	// Enemy가 접근을 마치고 펭귄이 달리는 지면 근처까지 내려온 뒤 사격합니다.
	// Chasing은 X축 접근이 끝나 사격 위치에 도달했다는 뜻입니다.
	if (coastEnemy->GetState() == EnemyState::Chasing)
	{
		enemyFireTimer += deltaTime;
		// 느린 예고탄, 한 번의 빠른 후속탄, 긴 휴식이 반복됩니다.
		constexpr float earlyFireIntervals[] = { 3.8f, 3.2f, 4.2f };
		constexpr float middleFireIntervals[] = { 3.0f, 2.1f, 3.4f };
		constexpr float lateFireIntervals[] = { 2.4f, 1.5f, 2.8f };
		constexpr int firePatternCount = 3;
		// UpdateRunSpeed와 같은 방식으로 코스 진행률에 따라 발사 간격을
		// 점점 줄여, 뒤쪽 해안 구간일수록 더 자주(연사에 가깝게) 쏘게 합니다.
		const float* fireIntervals = earlyFireIntervals;
		if (traveledDistance >= 10000.0f)
		{
			fireIntervals = lateFireIntervals;
		}
		else if (traveledDistance >= 5000.0f)
		{
			fireIntervals = middleFireIntervals;
		}
		const float fireInterval = fireIntervals[enemyFirePatternIndex];
		if (enemyFireTimer >= fireInterval)
		{
			const Craft::Vector2 enemyPosition = coastEnemy->GetPosition();
			const Craft::Vector2 bulletPosition(
				coastEnemyOnRight
					? enemyPosition.x
					: enemyPosition.x + coastEnemy->GetWidth(),
				GetPlayerScreenY());
			constexpr float bulletSpeed = 15.0f;
			enemyBullets.emplace_back(SpawnActor<EnemyBullet>(
				bulletPosition, coastEnemyOnRight ? -bulletSpeed : bulletSpeed));
			LogEvent("Enemy 사격 pattern="
				+ std::to_string(enemyFirePatternIndex));
			enemyFirePatternIndex =
				(enemyFirePatternIndex + 1) % firePatternCount;
			enemyFireTimer = 0.0f;
		}
	}
	else
	{
		enemyFireTimer = 0.0f;
	}
}

bool PolarLevel::IsNarrowIcePathAhead(float lookAheadDistance) const
{
	constexpr float sampleStep = 5.0f;
	const float safeLookAhead = (std::max)(0.0f, lookAheadDistance);
	const float endDistance = (std::min)(
		courseDistance, traveledDistance + safeLookAhead);

	for (float distance = traveledDistance;
		distance <= endDistance; distance += sampleStep)
	{
		if (GetRoadProfile(distance).terrain == TerrainType::NarrowIcePath)
		{
			return true;
		}
	}

	return GetRoadProfile(endDistance).terrain == TerrainType::NarrowIcePath;
}

void PolarLevel::HandlePlayerFire()
{
	if (!player || nonShotCount <= 0 || player->IsJumping())
	{
		return;
	}

	if (!Craft::Input::Get().GetKeyDown('F'))
	{
		return;
	}

	const int playerY = GetPlayerScreenY();
	const int playerX = GetRoadScreenX(
		player->GetHorizontalPosition(), playerY);


	const int direction = player->IsFacingRight() ? 1 : -1;
	const int bulletStartX = playerX + direction * 5;
	SpawnActor<PlayerBullet>(Craft::Vector2(bulletStartX, playerY - 1), direction);
	Craft::Engine::Get().PlayOneShot("Laser_Shoot.wav");

	--nonShotCount;

}

void PolarLevel::CheckStarCollections()
{
	// 별은 펭귄이 지면에서 같은 진행선과 레인에 있을 때만 수집합니다.
	// 점프 연출로 별의 화면 Y와 겹치는 경우는 수집하지 않습니다.
	if (!player || player->IsJumping())
	{
		return;
	}

	const int playerBaseY = GetPlayerScreenY() - player->GetJumpScreenOffset();
	const int playerBodyTopY = player->IsJumping()
		? playerBaseY - 3 : playerBaseY - 1;
	const int playerBodyBottomY = player->IsJumping()
		? playerBaseY - 1 : playerBaseY;
	for (const std::shared_ptr<PolarStar>& star : stars)
	{
		if (!star || star->IsCollected() || star->HasExpired())
		{
			continue;
		}

		const int currentY = DistanceToScreenY(star->GetDistance());
		// 현재 점프 자세의 몸통과 실제로 닿을 때만 수집합니다.
		// 정수 화면 좌표 변환으로 한 줄을 놓치지 않도록 1칸만 허용합니다.
		constexpr int collectionYTolerance = 1;
		const bool overlapsPlayerBody =
			currentY >= playerBodyTopY - collectionYTolerance
			&& currentY <= playerBodyBottomY + collectionYTolerance;

		// 화면 픽셀 폭이 아니라 같은 레인(정규화 좌표)에 있을 때만 수집되게
		// 합니다. 픽셀 폭 비교는 근접한 옆 레인에서도 스치듯 먹히곤 했습니다.
		constexpr float laneMatchTolerance = 0.25f;
		const bool horizontalOverlap =
			std::abs(player->GetHorizontalPosition()
				- star->GetHorizontalPosition()) < laneMatchTolerance;
		if (overlapsPlayerBody && horizontalOverlap)
		{
			star->Collect();
			Craft::Engine::Get().PlayOneShot("star.wav");
			++collectedStarCount;
			if (collectedStarCount >= RequiredStarCount)
			{
				collectedStarCount -= RequiredStarCount;
				++nonShotCount;
			}
		}
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
		const int previousObstacleScreenY =
			DistanceToScreenY(obstacle->GetPreviousDistance());
		const int currentPlayerScreenY = GetPlayerScreenY();
		const bool isBrokenBridge =
			obstacle->GetObstacleType() == ObstacleType::BrokenBridge;
		// BrokenBridge::Draw changes the water row according to perspective.
		// Use the same thresholds here; a fixed y - 1 contact row made the
		// medium-sized bridge collide one visible row before its water reached
		// the penguin.
		const auto getBridgeWaterScreenY = [this](float distance, int screenY)
		{
			const float closeness = 1.0f - distance / viewDistance;
			return closeness > 0.80f ? screenY - 1 : screenY;
		};
		const int contactScreenY = isBrokenBridge
			? getBridgeWaterScreenY(obstacle->GetDistance(), obstacleScreenY)
			: obstacleScreenY;
		const int previousContactScreenY = isBrokenBridge
			? getBridgeWaterScreenY(obstacle->GetPreviousDistance(),
				previousObstacleScreenY)
			: previousObstacleScreenY;
		if (obstacle->GetDistance() < -2.0f)
		{
			obstacle->MarkChecked();
			continue;
		}

		// Near the collision row, LowSpike is drawn as "^^^^". Match that visible
		// width against the sliding penguin's central body instead of estimating
		// the contact only with normalized road coordinates.
		const int playerCollisionX = GetRoadScreenX(
			player->GetHorizontalPosition(), currentPlayerScreenY);
		const int obstacleCollisionX = GetRoadScreenX(
			obstacle->GetHorizontalPosition(), currentPlayerScreenY);
		const bool spikeScreenOverlap =
			playerCollisionX - 2 <= obstacleCollisionX + 1
			&& playerCollisionX + 4 >= obstacleCollisionX - 2;
		const bool puddleScreenOverlap =
			playerCollisionX - 2 <= obstacleCollisionX + 3
			&& playerCollisionX + 4 >= obstacleCollisionX - 4;
		// IceWall은 거리에 따라 화면상 실제로 그려지는 폭(지붕 제외, 몸통만)이
		// 크게 달라져서, 정규화 좌표만 비교하면 눈에 보이는 것과 판정이
		// 어긋납니다(겹쳐 보이는데 안 맞거나, 반대로 안 겹쳐 보이는데 맞음).
		// Draw()가 그리는 실제 몸통 범위와, 플레이어의 몸통("(_____)") 범위를
		// 화면 좌표로 직접 비교합니다. 머리의 부리(">"/"<")는 몸통 문자열에
		// 아예 포함되지 않으므로 판정에서 자연히 제외됩니다.
		const ScreenBounds iceWallBounds = obstacle->GetIceWallScreenBounds();
		// Player::Draw()가 실제로 "(_____)"를 그리는 시작 X와 동일한 계산을
		// 씁니다. 판정 코드가 따로 계산하면 나중에 Draw() 쪽 값만 바뀌었을
		// 때 둘이 어긋날 수 있습니다.
		// 점프 자세의 몸통("/( _ )\\")은 방향과 관계없이 x - 3에서
		// 시작합니다. 미끄러지는 자세용 오프셋을 그대로 사용하면 왼쪽을
		// 보며 점프할 때 히트박스가 화면보다 오른쪽으로 밀립니다.
		const int playerBodyLeft = playerCollisionX
			+ (player->IsJumping() ? -3 : player->GetBodyLeftOffset());
		const int playerBodyRight = playerBodyLeft + (Player::BodyWidth - 1);
		// IceWall의 외곽선("|")도 실제 벽으로 취급합니다. 플레이어 몸통의
		// 괄호("("/")")가 외곽선에 닿는 순간부터 Crash입니다.
		// ( 와 | 사이에 빈 칸 있음 -> 안전 / ( 와 | 가 닿음 -> Crash /
		// ( 가 # 안으로 들어감 -> Crash
		const int iceWallSolidLeft = iceWallBounds.left;
		const int iceWallSolidRight = iceWallBounds.right;
		const bool iceWallScreenOverlap =
			playerBodyLeft <= iceWallSolidRight
			&& playerBodyRight >= iceWallSolidLeft;
		const ObstacleType obstacleType = obstacle->GetObstacleType();
		const bool horizontalOverlap = obstacleType == ObstacleType::LowSpike
			? spikeScreenOverlap
			: (obstacleType == ObstacleType::Puddle
				? puddleScreenOverlap : iceWallScreenOverlap);
		// Collision occurs only when the projected obstacle crosses the penguin's
		// foot row from above. The swept row test also works at high run speeds.
		const bool crossedPlayerRow =
			previousObstacleScreenY <= currentPlayerScreenY
			&& obstacleScreenY >= currentPlayerScreenY;
		// The blue water row is the actual gap. The white row above it is only a
		// visual crack, so it must not trigger an early fall.
		// Give the projected water row one visual cell to reach the penguin's
		// feet. Integer perspective projection can otherwise report the crossing
		// while a black row is still visible between the feet and the gap.
		const int bridgeFallScreenY = currentPlayerScreenY + 1;
		const bool crossedBridgeEntry =
			previousContactScreenY <= bridgeFallScreenY
			&& contactScreenY >= bridgeFallScreenY;
		// IceWall의 현재 화면상 몸통 범위와 플레이어의 실제 미끄러짐 몸통 줄을
		// 직접 비교합니다. 이전~현재 프레임 전체를 넓게 합치면 커브에서 벽이
		// 이미 옆으로 이동한 뒤에도 과거 Y 범위 때문에 오충돌이 발생합니다.
		// 화면 Y는 거리 투영과 점프 높이가 섞인 시각 좌표입니다. IceWall의
		// 높은 그림이 점프한 펭귄과 화면에서 겹쳐도 실제 진행 거리는 아직
		// 멀 수 있으므로, 충돌 시점은 장애물의 기준 행이 플레이어 진행선을
		// 통과했는지로 판정합니다.
		const bool reachesPlayer = isBrokenBridge
			? crossedBridgeEntry : crossedPlayerRow;
		const bool clearedLowSpike =
			obstacleType == ObstacleType::LowSpike
			&& player->IsAboveObstacle();
		const bool clearedPuddle = obstacleType == ObstacleType::Puddle
			&& player->GetJumpHeight() >= 0.25f;
		// 화면에서 펭귄이 3칸 이상 떠 있을 때 다리를 통과합니다.
		// 렌더링과 동일한 정수 화면 높이를 사용해, 충분히 떠 보이는데도
		// 부동소수점 높이 경계 때문에 추락하는 상황을 방지합니다.
		// A bridge gap has no physical height to clear. Any real airborne state
		// clears it; unlike the old jumpIntent check, holding Space alone does not
		// count because Player::IsJumping() is controlled by the fixed jump timer.
		const bool clearedBrokenBridge = isBrokenBridge
			&& player->IsJumping();
		if ((isBrokenBridge || horizontalOverlap) && reachesPlayer
			&& !clearedLowSpike && !clearedPuddle && !clearedBrokenBridge)
		{
			if (isBrokenBridge)
			{
				// The swept test may detect a crossing after a fast bridge has
				// already advanced several console rows. Freeze the crash display
				// with its near water row on the actual fall line instead of showing
				// the bridge well below the penguin.
				const int nearBridgeBaseY = bridgeFallScreenY + 1;
				obstacle->SetCollisionDisplayDistance(
					ScreenYToDistance(nearBridgeBaseY));
			}
			crashedObstacleType = obstacle->GetObstacleType();
			fellThroughBrokenBridge = isBrokenBridge;
			state = State::Crashed;
			Craft::Engine::Get().PlayOneShot("gameover.wav");
			std::ostringstream crashEntry;
			crashEntry << "CRASH: " << GetObstacleName(crashedObstacleType)
				<< " speed=" << runSpeed;
			if (obstacleType == ObstacleType::IceWall)
			{
				// 판정에 쓰인 화면 좌표를 잠시 같이 남겨, 실제 벽 몸통 위치와
				// 비교해볼 수 있게 합니다.
				crashEntry << " playerX=[" << playerBodyLeft << ","
					<< playerBodyRight << "] wallX=["
					<< iceWallBounds.left << "," << iceWallBounds.right << "]";
			}
			else if (isBrokenBridge)
			{
				crashEntry << " jumpHeight=" << player->GetJumpHeight()
					<< " jumpOffset=" << player->GetJumpScreenOffset();
			}
			LogEvent(crashEntry.str());
			return;
		}
		if ((!isBrokenBridge && obstacleScreenY > currentPlayerScreenY)
			|| (isBrokenBridge && contactScreenY > bridgeFallScreenY))
		{
			obstacle->MarkChecked();
		}
	}
}

void PolarLevel::CheckEnemyBulletCollisions()
{
	if (!player)
	{
		return;
	}

	const int playerGroundY = GetPlayerScreenY();

	const int playerY =
		playerGroundY - player->GetJumpScreenOffset();

	const int playerX = GetRoadScreenX(
		player->GetHorizontalPosition(),
		playerGroundY);

	const int playerLeft =
		playerX - (player->IsJumping() ? 3 : 5);

	const int playerRight =
		playerX + (player->IsJumping() ? 3 : 5);

	const int playerTop =
		playerY - (player->IsJumping() ? 4 : 1);

	const int playerBottom = playerY;

	for (const std::shared_ptr<EnemyBullet>& bullet : enemyBullets)
	{
		if (bullet && !bullet->HasExpired())
		{
			const Craft::Vector2 current = bullet->GetPosition();
			const Craft::Vector2 previous = bullet->GetPreviousPosition();
			const int bulletLeft = (std::min)(current.x, previous.x);
			const int bulletRight = (std::max)(current.x, previous.x);
			const bool overlapsX = bulletRight >= playerLeft
				&& bulletLeft <= playerRight;
			const bool overlapsY = current.y >= playerTop
				&& current.y <= playerBottom;

			if (overlapsX && overlapsY)
			{
				bullet->Destroy();
				hitByEnemyBullet = true;
				state = State::Crashed;
				Craft::Engine::Get().PlayOneShot("gameover.wav");
				LogEvent("CRASH: Enemy 탄환 피격 speed="
					+ std::to_string(runSpeed));
				return;
			}
		}
		if (!bullet || bullet->HasExpired())
		{
			continue;
		}

		// 다음 단계에서 탄환 위치와 비교합니다.
	}
}

bool PolarLevel::IsOnNarrowIcePath() const
{
	return GetRoadProfile(traveledDistance).terrain
		== TerrainType::NarrowIcePath;
}

void PolarLevel::CheckTerrainHazards(float deltaTime)
{
	if (!player || !IsOnNarrowIcePath())
	{
		narrowPathFallTimer = 0.0f;
		return;
	}

	constexpr float fallBoundary = 1.25f;
	constexpr float fallGraceTime = 0.4f;
	if (std::abs(player->GetHorizontalPosition()) > fallBoundary)
	{
		narrowPathFallTimer += deltaTime;
		if (narrowPathFallTimer >= fallGraceTime)
		{
			fellFromNarrowIcePath = true;
			state = State::Crashed;
			Craft::Engine::Get().PlayOneShot("gameover.wav");
			LogEvent("CRASH: 좁은 얼음길 추락 speed="
				+ std::to_string(runSpeed));
		}
	}
	else
	{
		narrowPathFallTimer = 0.0f;
	}
}

int PolarLevel::GetPlayerScreenY() const
{
	const int offset = player ? player->GetLongitudinalScreenOffset() : 0;
	return std::clamp(playerScreenY + offset, horizonY + 3, screenHeight - 2);
}

float PolarLevel::GetPlayerHorizontalMin() const
{
	const TerrainType terrain = GetRoadProfile(traveledDistance).terrain;
	if (terrain == TerrainType::Coast)
	{
		return -0.62f;
	}
	return terrain == TerrainType::NarrowIcePath ? -1.35f : -1.0f;
}

float PolarLevel::GetPlayerHorizontalMax() const
{
	const TerrainType terrain = GetRoadProfile(traveledDistance).terrain;
	return terrain == TerrainType::NarrowIcePath ? 1.35f : 1.0f;
}

const char* PolarLevel::GetObstacleName(ObstacleType type) const
{
	if (type == ObstacleType::IceWall) return "ICE WALL";
	if (type == ObstacleType::BrokenBridge) return "BROKEN BRIDGE";
	if (type == ObstacleType::Puddle) return "PUDDLE";
	return "LOW SPIKE";
}

int PolarLevel::DistanceToScreenY(float distance) const
{
	const float normalized = std::clamp(1.0f - distance / viewDistance, 0.0f, 1.0f);
	const float perspective = std::pow(normalized, 1.55f);
	const int roadTopY = horizonY + 1;
	const int roadBottomY = screenHeight - 1;
	return roadTopY + static_cast<int>(perspective * (roadBottomY - roadTopY));
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
	const int roadTopY = horizonY + 1;
	return std::clamp(static_cast<float>(screenY - roadTopY)
		/ (roadBottomY - roadTopY), 0.0f, 1.0f);
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

	// Keep the opening snowfield broad; narrow terrain still applies its own
	// width scale below, so canyon and bridge sections remain distinct.
	const int nearHalfWidth = static_cast<int>(screenWidth * 0.38f);
	const int perspectiveHalfWidth = 2
		+ static_cast<int>(depth * (nearHalfWidth - 2));
	const int maximumVisibleHalfWidth = (std::max)(2,
		(std::min)(slice.centerX - 1, screenWidth - slice.centerX - 2));
	const float widthBlend = depth * depth * (3.0f - 2.0f * depth);
	const float visibleWidth = 1.0f + (slice.width - 1.0f) * widthBlend;
	float minimumWidthScale = 0.72f;
	if (slice.terrain == TerrainType::Canyon)
	{
		minimumWidthScale = 0.45f;
	}
	else if (slice.terrain == TerrainType::NarrowIcePath)
	{
		minimumWidthScale = 0.28f;
	}
	const int minimumHalfWidth = 2 + static_cast<int>(depth
		* (nearHalfWidth * minimumWidthScale - 2.0f));
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
	const int usableHalfWidth = (std::max)(1,
		static_cast<int>(slice.halfWidth * 0.72f));
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
	// 보조 레벨(MenuLevel)이 켜져 있는 동안에는 Engine이 이 Tick 자체를
	// 호출하지 않으므로, 여기서는 메뉴를 "여는" 입력만 처리하면 된다.
	const Craft::Input& input = Craft::Input::Get();
	if (state == State::Playing)
	{
		if (input.GetKeyDown(VK_ESCAPE))
		{
			OpenMenu(true, MenuLevel::Item::Resume);
		}
		return;
	}

	if (state == State::Crashed || state == State::Goal)
	{
		if (input.GetKeyDown(VK_ESCAPE))
		{
			OpenMenu(false, MenuLevel::Item::Retry);
		}
		else if (input.GetKeyDown('R'))
		{
			RetryGame();
		}
	}
}

void PolarLevel::OpenMenu(bool canResume, MenuLevel::Item defaultItem)
{
	Craft::Engine& engine = Craft::Engine::Get();
	if (!engine.GetSecondaryLevel())
	{
		engine.SetSecondaryLevel<MenuLevel>(canResume, defaultItem);
	}
	else
	{
		std::static_pointer_cast<MenuLevel>(engine.GetSecondaryLevel())
			->SetContext(canResume, defaultItem);
	}
	engine.SetSecondaryLevelActive(true);
}

void PolarLevel::RetryGame()
{
	LogEvent("RETRY 요청 STAR=" + std::to_string(collectedStarCount)
		+ " SHOT=" + std::to_string(nonShotCount));
	Craft::Engine::Get().AddNewLevel<PolarLevel>();
}

const char* PolarLevel::GetCurveDirection() const
{
	if (curveStrength < -0.08f) return "LEFT";
	if (curveStrength > 0.08f) return "RIGHT";
	return "STRAIGHT";
}

void PolarLevel::UpdateRunSpeed(float deltaTime)
{
	constexpr float maximumForwardBonus = 2.0f;
	constexpr float maximumCurvePenalty = 2.0f;

	// 약 10분 코스 전체에서 속도가 계속 상승하도록 3000m 단위로
	// 목표 속도를 높입니다. 구간 내부는 보간하여 갑작스러운 변화는 피합니다.
	constexpr float speedDistances[] =
	{
		0.0f, 3000.0f, 6000.0f, 9000.0f, 12000.0f, 15000.0f
	};
	constexpr float speedValues[] =
	{
		16.0f, 20.0f, 24.0f, 28.0f, 32.0f, 35.0f
	};
	constexpr int speedPointCount = 6;
	float progressionSpeed = speedValues[speedPointCount - 1];
	for (int index = 1; index < speedPointCount; ++index)
	{
		if (traveledDistance <= speedDistances[index])
		{
			const float amount = std::clamp(
				(traveledDistance - speedDistances[index - 1])
				/ (speedDistances[index] - speedDistances[index - 1]),
				0.0f, 1.0f);
			progressionSpeed = speedValues[index - 1]
				+ (speedValues[index] - speedValues[index - 1]) * amount;
			break;
		}
	}
	const float forwardAmount = player ? std::clamp(
		-static_cast<float>(player->GetLongitudinalScreenOffset()) / 8.0f,
		0.0f, 1.0f) : 0.0f;
	const float curveAmount = std::clamp(
		std::abs(curveStrength) / 0.6f, 0.0f, 1.0f);
	const float targetSpeed = progressionSpeed
		+ forwardAmount * maximumForwardBonus
		- curveAmount * maximumCurvePenalty;
	const float speedBlend = (std::min)(deltaTime * 2.5f, 1.0f);
	runSpeed += (targetSpeed - runSpeed) * speedBlend;
}

void PolarLevel::UpdateSpeedNotification(float deltaTime)
{
	speedNotificationTimer = (std::max)(
		0.0f, speedNotificationTimer - deltaTime);

	constexpr float thresholds[] = { 3000.0f, 6000.0f, 9000.0f, 12000.0f };
	const char* messages[] =
	{
		"SPEED UP!", "SPEED UP!!", "HIGH SPEED!", "MAX SPEED!"
	};
	constexpr int notificationCount = 4;
	while (speedNotificationStage < notificationCount
		&& traveledDistance >= thresholds[speedNotificationStage])
	{
		speedNotification = messages[speedNotificationStage];
		speedNotificationTimer = 1.2f;
		++speedNotificationStage;
	}
}

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
	const bool hasVisibleEnemy = coastEnemy && !coastEnemy->HasExpired();
	// Enemy 제거 후 Coast를 한 번에 끄지 않습니다. 새 지형은 먼 지평선에서
	// 나타나 아래로 내려오고, 기존 바다는 가까운 화면 아래로 밀려납니다.
	// Enemy may be destroyed after this level's Tick during Actor collision
	// processing. Keep the coast on that removal frame too, preventing the
	// underlying course terrain from flashing for one frame.
	const bool keepCoastVisual = hasVisibleEnemy
		|| coastVisualHoldTimer > 0.0f
		|| coastEnemyWasActive;

	for (int y = roadTopY; y <= roadBottomY; ++y)
	{
		const float depth = ScreenYToRoadDepth(y);
		RoadSlice slice = CalculateRoadSlice(depth);
		// Enemy가 살아 있는 동안에는 다음 코스 지형으로 화면이 교체되지
		// 않도록 현재 도로 형태를 유지한 채 표면만 Coast로 출력합니다.
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
	// Enemy 구간 안이면 그 구간에서 정해둔 바다 방향을 따라가고,
	// 그 외 해안 구간은 기존처럼 왼쪽에 바다를 둡니다.
	bool oceanOnRight = false;
	if (coastOceanOnRight.empty())
	{
		// 추격 중에는 처음 등장했던 바다 방향을 그대로 유지합니다.
		oceanOnRight = false;
	}
	else for (int zoneIndex = 0; zoneIndex < kEnemyZoneCount; ++zoneIndex)
	{
		if (slice.distance >= kEnemyZoneStarts[zoneIndex]
			&& slice.distance <= kEnemyZoneEnds[zoneIndex])
		{
			oceanOnRight = coastOceanOnRight[zoneIndex];
			break;
		}
	}

	if (oceanOnRight)
	{
		DrawRoadEdges(y, slice, &PolarLevel::DrawSnowSurface, &PolarLevel::DrawOcean,
			depth, 0.45f, "\\", "\\", ":", "~",
			Craft::Color::BrightWhite, Craft::Color::Blue);
	}
	else
	{
		DrawRoadEdges(y, slice, &PolarLevel::DrawOcean, &PolarLevel::DrawSnowSurface,
			depth, 0.45f, ":", "~", "\\", "\\",
			Craft::Color::Blue, Craft::Color::BrightWhite);
	}
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
	hud << "DISTANCE: ";
	if (displayDistance < 1000.0f)
	{
		hud << static_cast<int>(displayDistance) << " m";
	}
	else
	{
		hud << std::fixed << std::setprecision(1)
			<< displayDistance / 1000.0f << " km";
	}
	hud << std::fixed << std::setprecision(1);

	constexpr int panelWidth = 30;
	const std::string border = "+" + std::string(panelWidth - 2, '-') + "+";
	Craft::Renderer::Get().Submit(border, Craft::Vector2(0, 0),
		Craft::Color::BrightWhite, 1000);
	Craft::Renderer::Get().Submit(border, Craft::Vector2(0, 5),
		Craft::Color::BrightWhite, 1000);
	for (int row = 1; row < 5; ++row)
	{
		Craft::Renderer::Get().Submit("|", Craft::Vector2(0, row),
			Craft::Color::BrightWhite, 1000);
		Craft::Renderer::Get().Submit("|", Craft::Vector2(panelWidth - 1, row),
			Craft::Color::BrightWhite, 1000);
	}

	const auto makeSlots = [](int count, int capacity, char filled)
	{
		std::string slots = "[";
		slots.append(std::clamp(count, 0, capacity), filled);
		slots.append(capacity - std::clamp(count, 0, capacity), '-');
		return slots + "]";
	};
	const std::string starHud = "STAR: "
		+ makeSlots(collectedStarCount, RequiredStarCount, '*')
		+ " " + std::to_string(collectedStarCount)
		+ "/" + std::to_string(RequiredStarCount);
	const std::string shotHud = "SHOT: "
		+ makeSlots(nonShotCount, 5, '@')
		+ " " + std::to_string(nonShotCount);

	std::ostringstream statusHud;
	statusHud << std::fixed << std::setprecision(1)
		<< "SPEED: " << runSpeed;

	Craft::Renderer::Get().Submit(hud.str(), Craft::Vector2(2, 1),
		Craft::Color::BrightWhite, 1000);
	Craft::Renderer::Get().Submit(statusHud.str(), Craft::Vector2(2, 2),
		Craft::Color::Cyan, 1000);
	Craft::Renderer::Get().Submit(starHud, Craft::Vector2(2, 3),
		Craft::Color::Yellow, 1000);
	Craft::Renderer::Get().Submit(shotHud, Craft::Vector2(2, 4),
		Craft::Color::Red, 1000);
	Craft::Renderer::Get().Submit(
		"SPACE Jump  F Fire  ESC Menu",
		Craft::Vector2(1, 6), Craft::Color::Cyan, 1000);
	if (speedNotificationTimer > 0.0f)
	{
		Craft::Renderer::Get().Submit(speedNotification,
			Craft::Vector2(screenWidth / 2
				- static_cast<int>(speedNotification.size()) / 2, 8),
			speedNotificationStage >= 3
				? Craft::Color::Yellow : Craft::Color::Cyan,
			1600);
	}
	if (coastEnemyWarningTimer > 0.0f)
	{
		const std::string warning = "ENEMY SHIP APPROACHING!";
		Craft::Renderer::Get().Submit(warning,
			Craft::Vector2(screenWidth / 2
				- static_cast<int>(warning.size()) / 2, 9),
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
				- static_cast<int>(warning.size()) / 2, 7),
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

void PolarLevel::Draw()
{
	DrawSkyAndHorizon();
	DrawPerspectiveRoad();
	DrawNarrowPathWarningSign();
	Level::Draw();
	DrawHud();
}
