#include "TestLevel.h"

#include <Actor/PlayerBullet.h>
#include <Actor/Enemy.h>
#include <Engine/Engine.h>
#include <Input/Input.h>
#include <Math/Color.h>
#include <Render/Renderer.h>
#include <Windows.h>

void TestLevel::Tick(float deltaTime)
{
	// 상위 틱 로직 호출.
	super::Tick(deltaTime);

	if (Craft::Input::Get().GetKeyDown('F'))
	{
		const int screenWidth =
			Craft::Engine::Get().GetWidth();

		const int screenHeight =
			Craft::Engine::Get().GetHeight();

		const Craft::Vector2 bulletPosition(
			screenWidth / 2,
			screenHeight / 2);

		SpawnActor<PlayerBullet>(bulletPosition, 1);
	}
}

void TestLevel::Draw()
{
	// SpawnActor로 생성한 PlayerBullet을 화면에 그린다.
	super::Draw();

	Craft::Renderer::Get().Submit(
		"SHOOT TEST - F : FIRE",
		Craft::Vector2(2, 2),
		Craft::Color::BrightWhite,
		1000);
}

void TestLevel::OnInitialized()
{
	super::OnInitialized();

	const int screenWidth = Craft::Engine::Get().GetWidth();
	const int screenHeight = Craft::Engine::Get().GetHeight();

	SpawnActor<Enemy>(
		Craft::Vector2(screenWidth - 15, screenHeight / 2));
}
