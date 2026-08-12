#pragma once

#include <Level/Level.h>

// PlayerBullet의 생성과 이동을 독립적으로 확인하는 테스트 레벨.
class TestLevel : public Craft::Level
{
	TYPE_DECLARATIONS(TestLevel, Craft::Level)

public:
	virtual void Tick(float deltaTime) override;
	virtual void Draw() override;
	virtual void OnInitialized() override;
};

