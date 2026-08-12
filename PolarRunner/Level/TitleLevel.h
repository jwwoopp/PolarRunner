#pragma once

#include <Level/Level.h>

class TitleLevel : public Craft::Level
{
public:
	virtual void Tick(float deltaTime) override;
	virtual void Draw() override;
};
