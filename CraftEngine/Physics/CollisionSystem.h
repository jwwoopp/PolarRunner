#pragma once

#include <vector>
#include <memory>

namespace Craft
{
	// 전방 선언.
	class Actor;

	// 액터 간 충돌을 테스트하는 클래스.
	class CollisionSystem
	{
		// 충돌 이벤트 발생을 위해 액터를 저장할 구조체.
		struct CollisionPair
		{
			std::shared_ptr<Actor> actor;
			std::shared_ptr<Actor> other;
		};

	public:
		CollisionSystem() = default;
		~CollisionSystem() = default;

		// 액터를 순회하면서 충돌을 확인하는 함수.
		void ProcessCollision(const std::vector<std::shared_ptr<Actor>>& actorList);

	private:

		// 두 액터가 충돌했는지 확인(테스트)하는 함수.
		bool Test(
			const std::shared_ptr<Actor>& left,
			const std::shared_ptr<Actor>& right
		);
	};
}