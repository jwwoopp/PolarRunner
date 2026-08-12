#pragma once

#include <Actor/Actor.h>

// 길 바깥에 등장하는 밀렵꾼 적.

enum class EnemySide
{
	Left,
	Right

};

class Enemy : public Craft::Actor
{
	TYPE_DECLARATIONS(Enemy, Craft::Actor)

public:
	Enemy(const Craft::Vector2& position);

	void Advance(float amount);

	inline float GetDistance() const
	{
		return distance;
	}

	inline EnemySide GetSide() const
	{
		return side;
	}

private:
	float distance = 0.0f;
	EnemySide side = EnemySide::Right;

};

