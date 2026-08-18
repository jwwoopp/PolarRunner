#include "TitleLevel.h"

#include <Engine/Engine.h>
#include <Input/Input.h>
#include <Level/PolarLevel.h>
#include <Level/TestLevel.h>
#include <Math/Color.h>
#include <Render/Renderer.h>
#include <Windows.h>
#include <cstring>
#include <string>

namespace
{
	using Craft::Color;
	using Craft::Vector2;

	// 로고. 역슬래시가 많아 raw string literal로 화면 모양 그대로 적는다.
	const char* kLogo[] =
	{
		R"( ____   ___  _        _    ____    ____  _   _ _   _ _   _ _____ ____  )",
		R"(|  _ \ / _ \| |      / \  |  _ \  |  _ \| | | | \ | | \ | | ____|  _ \ )",
		R"(| |_) | | | | |     / _ \ | |_) | | |_) | | | |  \| |  \| |  _| | |_) |)",
		R"(|  __/| |_| | |___ / ___ \|  _ <  |  _ <| |_| | |\  | |\  | |___|  _ < )",
		R"(|_|    \___/|_____/_/   \_\_| \_\ |_| \_\\___/|_| \_|_| \_|_____|_| \_\)",
	};

	constexpr int kLogoRowCount =
		static_cast<int>(sizeof(kLogo) / sizeof(kLogo[0]));

	void Put(const std::string& text, int x, int y, Color color, int order = 10)
	{
		Craft::Renderer::Get().Submit(text, Vector2(x, y), color, order);
	}
}

void TitleLevel::Tick(float deltaTime)
{
	Level::Tick(deltaTime);

	// 별 반짝임과 물결에 쓰는 시간만 누적한다. 게임 상태는 건드리지 않는다.
	animationTime += deltaTime;

	const Craft::Input& input = Craft::Input::Get();

	if (input.GetKeyDown(VK_RETURN))
	{
		// 엔터를 누르면 실제 게임 시작.
		PolarLevel::SetNextStartDistance(0.0f);
		Craft::Engine::Get().AddNewLevel<PolarLevel>();
	}
	else if (input.GetKeyDown('B'))
	{
		// 첫 BrokenBridge가 화면 앞쪽에 보이는 지점부터 시작합니다.
		PolarLevel::SetNextStartDistance(740.0f);
		Craft::Engine::Get().AddNewLevel<PolarLevel>();
	}
	else if (input.GetKeyDown('T'))
	{
		// T를 누르면 총알 테스트 화면 시작
		Craft::Engine::Get().AddNewLevel<TestLevel>();
	}
	else if (input.GetKeyDown(VK_ESCAPE))
	{
		Craft::Engine::Get().Quit();
	}
}

void TitleLevel::DrawLogo(int centerX)
{
	const int logoWidth = static_cast<int>(std::strlen(kLogo[0]));
	const int logoX = centerX - logoWidth / 2;
	for (int row = 0; row < kLogoRowCount; ++row)
	{
		Put(kLogo[row], logoX, 1 + row, Color::Cyan, 20);
	}

	const std::string tagline = "///   RUN  -  SLIDE  -  SURVIVE   ///";
	Put(tagline, centerX - static_cast<int>(tagline.size()) / 2, 7,
		Color::BrightWhite, 20);
}

void TitleLevel::DrawScene(int centerX)
{
	// 반짝이는 별. PolarLevel::DrawSkyAndHorizon과 같은 분위기를 유지한다.
	const bool twinkle = static_cast<int>(animationTime * 2.0f) % 2 == 0;
	Put(twinkle ? "*" : ".", 8, 9, Color::BrightWhite);
	Put(twinkle ? "." : "*", 27, 9, Color::BrightWhite);
	Put(twinkle ? "*" : ".", 55, 9, Color::BrightWhite);
	Put(".", 70, 9, Color::BrightWhite);

	// 원경 산맥과 지평선.
	Put("_/\\_      _/\\_        _/\\_", centerX - 13, 10, Color::Cyan);
	Put("/____\\____/____\\______/____\\", centerX - 14, 11, Color::BrightWhite);
	Put(std::string(80, '_'), 0, 12, Color::BrightWhite);

	// 원근 도로. 아래로 갈수록 넓어지는 실제 게임의 투영을 그대로 따라간다.
	for (int row = 0; row < 9; ++row)
	{
		const int y = 13 + row;
		Put("/", centerX - 2 - row * 2, y, Color::BrightWhite);
		Put("\\", centerX + 1 + row * 2, y, Color::BrightWhite);
	}

	// PolarStar::Draw()의 근접 이미지.
	Put("<*>", 33, 16, Color::Yellow, 15);

	// PolarObstacle::Draw()의 중거리 IceWall.
	Put(" /\\/\\ ", 43, 17, Color::Cyan, 15);
	Put("/####\\", 43, 18, Color::Cyan, 15);
	Put("|####|", 43, 19, Color::Cyan, 15);

	// LowSpike.
	Put("^^^^", 27, 20, Color::Yellow, 15);

	// Player::Draw()의 점프 자세를 그대로 사용한다.
	Put(" _~_ ", centerX - 2, 17, Color::Cyan, 30);
	Put(" (o o)", centerX - 3, 18, Color::BrightWhite, 30);
	Put(" / V \\", centerX - 3, 19, Color::BrightWhite, 30);
	Put("/( _ )\\", centerX - 3, 20, Color::BrightWhite, 30);
	Put("^ ^", centerX - 1, 21, Color::Yellow, 30);

	// Enemy::Draw()의 선체. 오른쪽에 있으므로 포문이 왼쪽(도로)을 향한다.
	const int shipBob = static_cast<int>(animationTime * 1.4f) % 2;
	Put("|+----+", 61, 14 + shipBob, Color::Red, 15);
	Put("|  ! ||", 61, 15 + shipBob, Color::Red, 15);
	Put("<==_O_|", 63, 17 + shipBob, Color::Red, 15);
	Put("/_____\\", 62, 18 + shipBob, Color::BrightWhite, 15);

	// 출렁이는 바다. 위상을 시간에 따라 밀어 물결을 만든다.
	const int wavePhase = static_cast<int>(animationTime * 4.0f);
	for (int row = 0; row < 3; ++row)
	{
		const int y = 19 + row;
		for (int x = 56 + row * 2; x < 79; ++x)
		{
			if ((x + wavePhase + row) % 3 != 0)
			{
				continue;
			}
			Put("~", x, y, Color::Blue);
		}
	}
}

void TitleLevel::DrawMenu(int centerX)
{
	Put(">  START GAME", centerX - 7, 23, Color::Yellow, 20);
	Put("   BRIDGE TEST", centerX - 7, 24, Color::Cyan, 20);
	Put("   SHOOT TEST", centerX - 7, 25, Color::Cyan, 20);
	Put("   QUIT", centerX - 7, 26, Color::BrightWhite, 20);

	const std::string hint =
		"[ ENTER ] START   [ B ] BRIDGE   [ T ] SHOOT   [ ESC ] QUIT";
	Put(hint, centerX - static_cast<int>(hint.size()) / 2, 28,
		Color::BrightWhite, 20);
}

void TitleLevel::Draw()
{
	const int centerX = Craft::Engine::Get().GetWidth() / 2;
	DrawLogo(centerX);
	DrawScene(centerX);
	DrawMenu(centerX);
}
