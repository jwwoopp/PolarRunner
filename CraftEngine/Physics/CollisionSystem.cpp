#include "CollisionSystem.h"
#include "Actor/Actor.h"

namespace Craft
{
	void CollisionSystem::ProcessCollision(
		const std::vector<std::shared_ptr<Actor>>& actorList)
	{
		// 예외처리.
		if (actorList.empty())
		{
			return;
		}

		// 충돌한 액터에 이벤트를 한번에 정리해 전달하기 위한 배열.
		std::vector<CollisionPair> collidedActorList;

		// 레벨에 배치된 액터 수.
		const int count = static_cast<int>(actorList.size());

		// 모든 액터를 순회하면서 충돌 검사.
		for (int ix = 0; ix < count; ++ix)
		{
			const std::shared_ptr<Actor>& left = actorList[ix];
			if (!left || !left->IsActive())
			{
				continue;
			}

			for (int jx = ix + 1; jx < count; ++jx)
			{
				const std::shared_ptr<Actor>& right = actorList[jx];
				if (!right || !right->IsActive())
				{
					continue;
				}

				// 충돌 검사.
				if (Test(left, right))
				{
					// 이벤트 발행할 목록에 추가할 데이터 생성.
					CollisionPair pair = {};
					pair.actor = left;
					pair.other = right;

					// 목록에 추가.
					collidedActorList.emplace_back(pair);
				}
			}
		}

		// 충돌 발생한 액터 목록 확인. 충돌한 액터가 없으면 함수 종료.
		if (collidedActorList.empty())
		{
			return;
		}

		// 충돌한 액터에 이벤트 전달.
		for (const CollisionPair& pair : collidedActorList)
		{
			// 이미 삭제되거나 비활성화된 액터는 제외.
			if (!pair.actor->IsActive() || !pair.other->IsActive())
			{
				continue;
			}

			// 충돌 이벤트 전달.
			pair.actor->OnCollision(pair.other);
			pair.other->OnCollision(pair.actor);
		}
	}

	bool CollisionSystem::Test(
		const std::shared_ptr<Actor>& left,
		const std::shared_ptr<Actor>& right)
	{
		if (!left || !right)
		{
			return false;
		}

		// AABB (Axis Aligned Bounding Box).
		// y는 크기가 1이기 때문에 x좌표만 고려.

		// left 액터의 현재/이전 위치.
		const Vector2 leftCurrent = left->GetPosition();
		const Vector2 leftPrevious = left->GetPreviousPosition();

		// right 액터의 현재/이전 위치.
		const Vector2 rightCurrent = right->GetPosition();
		const Vector2 rightPrevious = right->GetPreviousPosition();

		// 이전 프레임 위치와 현재 위치를 모두 포함하는 swept bounds 계산.
		const int leftXMin = (leftCurrent.x < leftPrevious.x) ? leftCurrent.x : leftPrevious.x;
		const int leftXMaxCurrent = leftCurrent.x + left->GetWidth() - 1;
		const int leftXMaxPrevious = leftPrevious.x + left->GetWidth() - 1;
		const int leftXMax = (leftXMaxCurrent > leftXMaxPrevious) ? leftXMaxCurrent : leftXMaxPrevious;

		const int rightXMin = (rightCurrent.x < rightPrevious.x) ? rightCurrent.x : rightPrevious.x;
		const int rightXMaxCurrent = rightCurrent.x + right->GetWidth() - 1;
		const int rightXMaxPrevious = rightPrevious.x + right->GetWidth() - 1;
		const int rightXMax = (rightXMaxCurrent > rightXMaxPrevious) ? rightXMaxCurrent : rightXMaxPrevious;

		// X좌표 기준으로 충돌이 발생할 수 없는 상황 처리.
		if (rightXMin > leftXMax)
		{
			return false;
		}

		if (rightXMax < leftXMin)
		{
			return false;
		}

		// 이전 프레임까지 고려한 y 충돌 영역 계산.
		const int leftYMin = (leftCurrent.y < leftPrevious.y) ? leftCurrent.y : leftPrevious.y;
		const int leftYMax = (leftCurrent.y > leftPrevious.y) ? leftCurrent.y : leftPrevious.y;

		const int rightYMin = (rightCurrent.y < rightPrevious.y) ? rightCurrent.y : rightPrevious.y;
		const int rightYMax = (rightCurrent.y > rightPrevious.y) ? rightCurrent.y : rightPrevious.y;

		// y좌표 기준으로 충돌이 발생할 수 없는 상황 처리.
		if (rightYMin > leftYMax)
		{
			return false;
		}

		if (rightYMax < leftYMin)
		{
			return false;
		}

		// 충돌 발생.
		return true;
	}
}