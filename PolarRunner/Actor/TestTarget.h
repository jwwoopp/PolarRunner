#pragma once

#include <Actor/Actor.h>

class TestTarget : public Craft::Actor
{
	TYPE_DECLARATIONS(TestTarget, Craft::Actor)

public:
	TestTarget(const Craft::Vector2& position);
};

