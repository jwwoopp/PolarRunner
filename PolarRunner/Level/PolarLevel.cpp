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
#include <iomanip>
#include <random>
#include <sstream>
#include <utility>

void PolarLevel::OnInitialized()
{
	Level::OnInitialized();
	screenWidth = Craft::Engine::Get().GetWidth();
	screenHeight = Craft::Engine::Get().GetHeight();
	horizonY = 7;
	playerScreenY = screenHeight - 6;
	player = SpawnActor<Player>();
	BuildRoadCourse();
	BuildTestCourse();
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
		{ 1000.0f, 0.00f, 1.35f, TerrainType::Snowfield }
	};
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
	const float lanes[] = { -0.55f, 0.0f, 0.55f };
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
	// Obstacles are finalized before collectibles. Each star then selects a lane
	// with enough same-lane clearance; obstacles in other lanes remain allowed.
	const float starDistances[] =
	{
		90.0f, 195.0f, 315.0f, 450.0f, 575.0f, 750.0f, 890.0f
	};
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
				lanes[laneIndex], starDistance));
		}
	}
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
	UpdateCoastEnemy(deltaTime);
	UpdateSpeedNotification(deltaTime);
	CheckTerrainHazards();
	CheckObstacleCollisions();
	CheckStarCollections();
	CheckEnemyBulletCollisions();
	if (traveledDistance >= courseDistance && IsPlaying())
	{
		state = State::Goal;
	}
}

void PolarLevel::UpdateCoastEnemy(float deltaTime)
{
	const TerrainType terrain = GetRoadProfile(traveledDistance).terrain;
	if (!hasSpawnedCoastEnemy && terrain == TerrainType::Coast
		&& nonShotCount > 0)
	{
		coastEnemyWarningTimer += deltaTime;
		constexpr float warningDuration = 2.0f;
		if (coastEnemyWarningTimer >= warningDuration)
		{
			constexpr float initialDistance = 95.0f;
			coastEnemy = SpawnActor<Enemy>(initialDistance, EnemySide::Left);
			coastEnemyScreenX = screenWidth * 0.18f;
			hasSpawnedCoastEnemy = true;
			coastEnemyWarningTimer = 0.0f;
		}
	}
	else if (!hasSpawnedCoastEnemy)
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
	const int roadLeftX = roadCenterX - roadHalfWidth;
	const int maximumEnemyX = roadLeftX
		- coastEnemy->GetWidth() - coastMargin;
	const int desiredEnemyX = (std::min)(
		playerScreenX - 25, maximumEnemyX);

	if (coastEnemy->GetState() == EnemyState::ClosingSide)
	{
		constexpr float sideApproachSpeed = 18.0f;
		coastEnemyScreenX += sideApproachSpeed * deltaTime;
		if (coastEnemyScreenX >= desiredEnemyX)
		{
			coastEnemyScreenX = static_cast<float>(desiredEnemyX);
			coastEnemy->BeginChasing();
		}
	}
	else if (coastEnemy->GetState() == EnemyState::Chasing)
	{
		const float blend = (std::min)(deltaTime * 5.0f, 1.0f);
		coastEnemyScreenX +=
			(desiredEnemyX - coastEnemyScreenX) * blend;
	}

	const int screenX = static_cast<int>(coastEnemyScreenX);
	coastEnemy->SetPosition(Craft::Vector2(screenX, screenY));

	// Enemy가 접근을 마치고 펭귄이 달리는 지면 근처까지 내려온 뒤 사격합니다.
	// Chasing은 X축 접근이 끝나 사격 위치에 도달했다는 뜻입니다.
	if (coastEnemy->GetState() == EnemyState::Chasing)
	{
		enemyFireTimer += deltaTime;
		constexpr float fireInterval = 2.0f;
		if (enemyFireTimer >= fireInterval)
		{
			const Craft::Vector2 enemyPosition = coastEnemy->GetPosition();
			const Craft::Vector2 bulletPosition(
				enemyPosition.x + coastEnemy->GetWidth(),
				GetPlayerScreenY());
			enemyBullets.emplace_back(
				SpawnActor<EnemyBullet>(bulletPosition));
			enemyFireTimer = 0.0f;
		}
	}
	else
	{
		enemyFireTimer = 0.0f;
	}
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

	--nonShotCount;

}

void PolarLevel::CheckStarCollections()
{
	if (!player)
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

		// 논리 레인이 아니라 실제 화면 X와 펭귄 ASCII 폭을 비교합니다.
		const int playerScreenX = GetRoadScreenX(
			player->GetHorizontalPosition(), GetPlayerScreenY());
		const int starScreenX = GetRoadScreenX(
			star->GetHorizontalPosition(), currentY);
		const int playerHalfWidth = player->IsJumping() ? 3 : 5;
		const bool horizontalOverlap =
			std::abs(playerScreenX - starScreenX) <= playerHalfWidth;
		if (overlapsPlayerBody && horizontalOverlap)
		{
			star->Collect();
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
		const int contactScreenY = isBrokenBridge
			? obstacleScreenY - 1
			: obstacleScreenY;
		const int previousContactScreenY = isBrokenBridge
			? previousObstacleScreenY - 1
			: previousObstacleScreenY;
		if (obstacle->GetDistance() < -2.0f)
		{
			obstacle->MarkChecked();
			continue;
		}

		// Compare positions in the shared road coordinate system. Comparing two
		// projected screen X values made nearby rows diverge on curved roads.
		const float combinedHalfWidth = player->GetHorizontalHalfWidth()
			+ obstacle->GetHorizontalHalfWidth();
		const bool normalizedHorizontalOverlap =
			std::abs(player->GetHorizontalPosition()
				- obstacle->GetHorizontalPosition()) <= combinedHalfWidth;
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
		const ObstacleType obstacleType = obstacle->GetObstacleType();
		const bool horizontalOverlap = obstacleType == ObstacleType::LowSpike
			? spikeScreenOverlap
			: (obstacleType == ObstacleType::Puddle
				? puddleScreenOverlap : normalizedHorizontalOverlap);
		// Collision occurs only when the projected obstacle crosses the penguin's
		// foot row from above. The swept row test also works at high run speeds.
		const bool crossedPlayerRow =
			previousObstacleScreenY <= currentPlayerScreenY
			&& obstacleScreenY >= currentPlayerScreenY;
		// The blue water row is the actual gap. The white row above it is only a
		// visual crack, so it must not trigger an early fall.
		const bool crossedBridgeEntry =
			previousContactScreenY <= currentPlayerScreenY
			&& contactScreenY >= currentPlayerScreenY;
		const bool reachesPlayer = isBrokenBridge
			? crossedBridgeEntry
			: crossedPlayerRow;
		const bool clearedLowSpike =
			obstacleType == ObstacleType::LowSpike
			&& player->IsAboveObstacle();
		const bool clearedPuddle = obstacleType == ObstacleType::Puddle
			&& player->GetJumpHeight() >= 0.25f;
		// 다리는 오직 펭귄이 충분히 뛰었을 때만 건너는 것이 가능(점프 높이 3 이상).
		const bool clearedBrokenBridge = isBrokenBridge
			&& player->GetJumpHeight() >= 0.6f;
		if ((isBrokenBridge || horizontalOverlap) && reachesPlayer
			&& !clearedLowSpike && !clearedPuddle && !clearedBrokenBridge)
		{
			crashedObstacleType = obstacle->GetObstacleType();
			fellThroughBrokenBridge = isBrokenBridge;
			state = State::Crashed;
			return;
		}
		if ((!isBrokenBridge && obstacleScreenY > currentPlayerScreenY)
			|| (isBrokenBridge && contactScreenY > currentPlayerScreenY))
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

void PolarLevel::CheckTerrainHazards()
{
	if (!player || !IsOnNarrowIcePath())
	{
		return;
	}

	constexpr float fallBoundary = 1.25f;
	if (std::abs(player->GetHorizontalPosition()) > fallBoundary)
	{
		fellFromNarrowIcePath = true;
		state = State::Crashed;
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
	const Craft::Input& input = Craft::Input::Get();
	if (state == State::Playing)
	{
		if (input.GetKeyDown(VK_ESCAPE))
		{
			selectedMenuItem = MenuItem::Resume;
			stateBeforePause = State::Playing;
			state = State::PauseMenu;
		}
		return;
	}

	if (state == State::PauseMenu)
	{
		if (input.GetKeyDown(VK_ESCAPE))
		{
			state = stateBeforePause;
			return;
		}

		const int itemCount = static_cast<int>(MenuItem::Length);
		int selectedIndex = static_cast<int>(selectedMenuItem);
		if (input.GetKeyDown(VK_UP))
		{
			selectedIndex = (selectedIndex + itemCount - 1) % itemCount;
		}
		else if (input.GetKeyDown(VK_DOWN))
		{
			selectedIndex = (selectedIndex + 1) % itemCount;
		}
		selectedMenuItem = static_cast<MenuItem>(selectedIndex);

		if (input.GetKeyDown(VK_RETURN))
		{
			switch (selectedMenuItem)
			{
			case MenuItem::Resume:
				state = stateBeforePause;
				break;
			case MenuItem::Retry:
				RetryGame();
				break;
			case MenuItem::Quit:
				Craft::Engine::Get().Quit();
				break;
			default:
				break;
			}
		}
		return;
	}

	if (state == State::Crashed || state == State::Goal)
	{
		if (input.GetKeyDown(VK_ESCAPE))
		{
			stateBeforePause = state;
			selectedMenuItem = MenuItem::Retry;
			state = State::PauseMenu;
		}
		else if (input.GetKeyDown('R'))
		{
			RetryGame();
		}
	}
}

void PolarLevel::RetryGame()
{
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
	constexpr float startSpeed = 16.0f;
	constexpr float maximumProgressSpeed = 28.0f;
	constexpr float maximumForwardBonus = 2.0f;
	constexpr float maximumCurvePenalty = 2.0f;

	const float courseProgress = std::clamp(
		traveledDistance / courseDistance, 0.0f, 1.0f);
	const float progressionSpeed = startSpeed
		+ (maximumProgressSpeed - startSpeed) * courseProgress;
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

	constexpr float thresholds[] = { 250.0f, 500.0f, 750.0f };
	const char* messages[] = { "SPEED UP!", "SPEED UP!!", "HIGH SPEED!" };
	while (speedNotificationStage < 3
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
	const int leftX = slice.centerX - slice.halfWidth;
	const int rightX = slice.centerX + slice.halfWidth;
	DrawSnowSurface(y, 0, leftX - 1);
	DrawSnowSurface(y, rightX + 1, screenWidth - 1);
	Craft::Renderer::Get().Submit(depth > 0.55f ? "/" : "|",
		Craft::Vector2(leftX, y), Craft::Color::BrightWhite, 0);
	Craft::Renderer::Get().Submit(depth > 0.55f ? "\\" : "|",
		Craft::Vector2(rightX, y), Craft::Color::BrightWhite, 0);
}

void PolarLevel::DrawCoastRow(int y, float depth, const RoadSlice& slice)
{
	const int leftX = slice.centerX - slice.halfWidth;
	const int rightX = slice.centerX + slice.halfWidth;
	DrawOcean(y, 0, leftX - 1);
	DrawSnowSurface(y, rightX + 1, screenWidth - 1);
	Craft::Renderer::Get().Submit(depth > 0.45f ? "~" : ":",
		Craft::Vector2(leftX, y), Craft::Color::Blue, 0);
	Craft::Renderer::Get().Submit("\\", Craft::Vector2(rightX, y),
		Craft::Color::BrightWhite, 0);
}

void PolarLevel::DrawCanyonRow(int y, float depth, const RoadSlice& slice)
{
	const int leftX = slice.centerX - slice.halfWidth;
	const int rightX = slice.centerX + slice.halfWidth;
	DrawCanyonWall(y, 0, leftX - 1);
	DrawCanyonWall(y, rightX + 1, screenWidth - 1);
	const char* wallEdge = depth > 0.5f ? "#" : "|";
	Craft::Renderer::Get().Submit(wallEdge, Craft::Vector2(leftX, y),
		Craft::Color::Cyan, 0);
	Craft::Renderer::Get().Submit(wallEdge, Craft::Vector2(rightX, y),
		Craft::Color::Cyan, 0);
}

void PolarLevel::DrawNarrowIcePathRow(int y, float depth, const RoadSlice& slice)
{
	const int leftX = slice.centerX - slice.halfWidth;
	const int rightX = slice.centerX + slice.halfWidth;
	DrawOcean(y, 0, leftX - 1);
	DrawOcean(y, rightX + 1, screenWidth - 1);

	// Perspective rails make the strip read as a bridge rather than a tunnel.
	Craft::Renderer::Get().Submit("/", Craft::Vector2(leftX, y),
		Craft::Color::BrightWhite, 1);
	Craft::Renderer::Get().Submit("\\", Craft::Vector2(rightX, y),
		Craft::Color::BrightWhite, 1);

	// Keep the intact ice quiet so the dense blue gap is immediately readable.
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
	const int leftX = slice.centerX - slice.halfWidth;
	const int rightX = slice.centerX + slice.halfWidth;
	DrawOcean(y, 0, leftX - 1);
	DrawOcean(y, rightX + 1, screenWidth - 1);
	DrawBrokenIce(y, leftX + 1, rightX - 1);
	Craft::Renderer::Get().Submit(depth > 0.5f ? "/" : "|",
		Craft::Vector2(leftX, y), Craft::Color::Cyan, 0);
	Craft::Renderer::Get().Submit(depth > 0.5f ? "\\" : "|",
		Craft::Vector2(rightX, y), Craft::Color::Cyan, 0);
}

void PolarLevel::DrawResearchBaseRow(int y, float depth, const RoadSlice& slice)
{
	const int leftX = slice.centerX - slice.halfWidth;
	const int rightX = slice.centerX + slice.halfWidth;
	DrawResearchBase(y, 0, leftX - 1);
	DrawResearchBase(y, rightX + 1, screenWidth - 1);
	const char* leftEdge = depth > 0.45f ? "[" : "|";
	const char* rightEdge = depth > 0.45f ? "]" : "|";
	Craft::Renderer::Get().Submit(leftEdge, Craft::Vector2(leftX, y),
		Craft::Color::BrightWhite, 0);
	Craft::Renderer::Get().Submit(rightEdge, Craft::Vector2(rightX, y),
		Craft::Color::BrightWhite, 0);
}

void PolarLevel::DrawCanyonWall(int y, int startX, int endX)
{
	startX = std::clamp(startX, 0, screenWidth - 1);
	endX = std::clamp(endX, 0, screenWidth - 1);
	for (int x = startX; x <= endX; ++x)
	{
		if ((x + y) % 2 == 0)
		{
			const char* glyph = (x + y) % 6 == 0 ? "#" : ".";
			Craft::Renderer::Get().Submit(glyph, Craft::Vector2(x, y),
				Craft::Color::Cyan, 0);
		}
	}
}

void PolarLevel::DrawSnowSurface(int y, int startX, int endX)
{
	startX = std::clamp(startX, 0, screenWidth - 1);
	endX = std::clamp(endX, 0, screenWidth - 1);
	for (int x = startX; x <= endX; ++x)
	{
		if ((x + y * 2) % 5 == 0)
		{
			const char* glyph = (x + y) % 10 == 0 ? "*" : ".";
			Craft::Renderer::Get().Submit(glyph, Craft::Vector2(x, y),
				Craft::Color::BrightWhite, 0);
		}
	}
}

void PolarLevel::DrawOcean(int y, int startX, int endX)
{
	startX = std::clamp(startX, 0, screenWidth - 1);
	endX = std::clamp(endX, 0, screenWidth - 1);
	for (int x = startX; x <= endX; ++x)
	{
		if ((x + y) % 3 == 0)
		{
			const char* glyph = (x + y) % 6 == 0 ? "~" : "-";
			Craft::Renderer::Get().Submit(glyph, Craft::Vector2(x, y),
				Craft::Color::Blue, 0);
		}
	}
}

void PolarLevel::DrawBrokenIce(int y, int startX, int endX)
{
	startX = std::clamp(startX, 0, screenWidth - 1);
	endX = std::clamp(endX, 0, screenWidth - 1);
	const float depth = ScreenYToRoadDepth(y);
	const int spacing = 17 - static_cast<int>(depth * 9.0f);
	for (int x = startX; x <= endX; ++x)
	{
		if ((x * 3 + y * 5) % spacing == 0)
		{
			const char* glyph = (x + y) % 2 == 0 ? "/" : "\\";
			Craft::Renderer::Get().Submit(glyph, Craft::Vector2(x, y),
				Craft::Color::Cyan, 0);
		}
	}
}

void PolarLevel::DrawResearchBase(int y, int startX, int endX)
{
	startX = std::clamp(startX, 0, screenWidth - 1);
	endX = std::clamp(endX, 0, screenWidth - 1);
	const float depth = ScreenYToRoadDepth(y);
	const int spacing = 10 - static_cast<int>(depth * 5.0f);
	for (int x = startX; x <= endX; ++x)
	{
		if ((x + y * 2) % spacing == 0)
		{
			const char* glyph = (y % 4 == 0) ? "#" : ((x % 5 == 0) ? "|" : "_");
			Craft::Renderer::Get().Submit(glyph, Craft::Vector2(x, y),
				Craft::Color::BrightWhite, 0);
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
	if (!hasSpawnedCoastEnemy && coastEnemyWarningTimer > 0.0f)
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
