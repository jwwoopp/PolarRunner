#pragma once

#include <Actor/Actor.h>

// 길 바깥에 등장하는 밀렵꾼 적.

enum class EnemySide
{
	Left,
	Right

};

enum class EnemyState
{
	ApproachingDepth,
	ClosingSide,
	Chasing
};

class Enemy : public Craft::Actor
{
	TYPE_DECLARATIONS(Enemy, Craft::Actor)

public:
	Enemy(const Craft::Vector2& position);
	Enemy(float distance, EnemySide side);

	void Advance(float amount, float chaseDistance);
	virtual void Draw() override;

	inline float GetDistance() const
	{
		return distance;
	}

	inline EnemySide GetSide() const
	{
		return side;
	}

	inline bool HasFinishedApproach() const
	{
		return state != EnemyState::ApproachingDepth;
	}

	inline EnemyState GetState() const { return state; }
	inline void BeginChasing() { state = EnemyState::Chasing; }

private:
	float distance = 0.0f;
	EnemySide side = EnemySide::Right;
	EnemyState state = EnemyState::ApproachingDepth;

};

