# PolarRunner 트러블슈팅 — 2026-08-12

## BrokenBridge 점프 판정 개선

### 문제 현상

- 끊어진 다리에서 펭귄이 충분히 높이 점프하지 않아도 통과할 수 있었다.
- `Space`를 계속 누르거나 `위 방향키 + Space`를 함께 사용할 때 다리를 쉽게 건너는 꼼수가 발생했다.
- ASCII 화면에서 보이는 펭귄의 높이와 실제 통과 판정이 일치하지 않았다.

### 원인

기존 코드는 실제 점프 높이가 아니라 점프 상태와 키 입력을 기준으로 다리 통과 여부를 판단했다.

```cpp
const bool jumpIntent = player->IsJumping()
    || Craft::Input::Get().GetKey(VK_SPACE)
    || Craft::Input::Get().GetKeyDown(VK_SPACE);

const bool clearedBrokenBridge = isBrokenBridge && jumpIntent;
```

이 방식에서는 점프를 막 시작했거나 거의 착지한 상태라도 `isJumping`이 참이면 다리를 통과할 수 있다. 또한 `Space`를 누르고 있다는 사실만으로도 통과 조건이 충족될 수 있다.

### 1차 해결

다리 통과 조건을 입력 상태가 아닌 실제 화면상 점프 높이로 변경했다.

```cpp
const bool clearedBrokenBridge = isBrokenBridge
    && player->GetJumpScreenOffset() >= 3;
```

이를 통해 펭귄이 일정 높이 이상 올라갔을 때만 다리를 통과하도록 만들었다.

### 추가 문제

`GetJumpScreenOffset()`은 콘솔 화면에서 펭귄을 몇 줄 위에 그릴지 나타내는 렌더링 값이다. 이 값을 충돌 판정에도 직접 사용하면 다음 문제가 생긴다.

- ASCII 모양이나 최대 출력 높이를 바꾸면 게임 난이도까지 달라진다.
- 콘솔 크기와 렌더링 방식이 게임 규칙에 영향을 줄 수 있다.
- 캐릭터 그림의 발 위치를 문자 단위로 계속 보정해야 할 가능성이 생긴다.

### 최종 해결

게임에서 사용하는 논리적 점프 높이와 ASCII 출력용 위치를 분리했다.

```cpp
float Player::GetJumpHeight() const
{
    if (!isJumping)
    {
        return 0.0f;
    }

    const float progress = jumpTimer / jumpDuration;
    return std::sin(progress * 3.14159265f);
}

int Player::GetJumpScreenOffset() const
{
    return static_cast<int>(GetJumpHeight() * 5.0f);
}
```

`GetJumpHeight()`는 점프 진행에 따라 `0.0~1.0` 범위의 논리 높이를 반환한다. 화면에서는 이 값에 `5`를 곱하여 펭귄의 ASCII 출력 위치를 계산한다.

장애물 판정은 다음과 같이 논리 높이를 사용한다.

```cpp
// 낮은 가시
return GetJumpHeight() >= 0.4f;

// 끊어진 다리
const bool clearedBrokenBridge = isBrokenBridge
    && player->GetJumpHeight() >= 0.6f;
```

### 변경 후 구조

```text
점프 시간
    ↓
GetJumpHeight() — 게임 판정용 높이(0.0~1.0)
    ├─ LowSpike 통과 판정: 0.4 이상
    ├─ BrokenBridge 통과 판정: 0.6 이상
    └─ GetJumpScreenOffset(): ASCII 출력 위치로 변환
```

게임 로직은 논리 높이를 사용하고, 화면 출력만 콘솔 좌표를 사용하므로 펭귄 ASCII를 수정해도 충돌 규칙이 바뀌지 않는다.

### 검증 결과

- Debug x64 빌드 성공
- 빌드 오류 없음
- `Space` 입력 여부가 BrokenBridge 통과 조건에서 제거됨
- LowSpike와 BrokenBridge가 서로 다른 통과 높이를 사용함

### 추가 플레이 테스트 항목

- BrokenBridge의 `0.6` 기준이 후반 속도에서도 공정한지 확인
- LowSpike의 `0.4` 기준이 너무 쉽거나 어렵지 않은지 확인
- 화면상 펭귄의 발과 장애물 접촉 시점이 자연스러운지 확인
- 1000m 코스에서 점프할 수 없는 장애물 배치가 생성되지 않는지 확인

### 배운 점

- 입력 상태와 실제 액션 결과는 분리해야 한다.
- 충돌 판정을 ASCII 문자의 개별 위치에 직접 연결하지 않는다.
- 게임 규칙은 논리 좌표로 계산하고, 렌더링 단계에서만 화면 좌표로 변환한다.
- 장애물마다 요구 높이를 다르게 두면 같은 점프 동작으로도 난이도 차이를 만들 수 있다.

---

## LowSpike 충돌 시점 개선

### 문제 현상

- 가시가 펭귄에게 닿기 전에 `CRASH: LOW SPIKE`가 표시됐다.
- 기존 판정을 수정한 뒤에는 반대로 가시가 펭귄을 그대로 통과하는 문제가 발생했다.

### 최초 원인

기존 코드는 장애물과 플레이어의 화면 Y 좌표 차이가 1 이하이면 충돌한 것으로 판단했다.

```cpp
const bool reachesPlayer =
    std::abs(contactScreenY - currentPlayerScreenY) <= 1;
```

고속 이동 중 충돌을 놓치지 않으려는 여유 범위였지만, 가시가 아직 플레이어보다 한 줄 앞에 있을 때도 충돌하여 조기 판정처럼 보였다.

### 실패한 접근

플레이어 화면 행을 거리로 역변환하고 장애물의 이전 거리와 현재 거리 사이에 포함되는지 검사했다.

```cpp
const float playerCollisionDistance =
    ScreenYToDistance(currentPlayerScreenY);

const bool crossedPlayerDistance =
    obstacle->GetPreviousDistance() >= playerCollisionDistance
    && obstacle->GetDistance() <= playerCollisionDistance;
```

그러나 현재 게임은 원근 투영 과정에서 화면 Y 좌표가 정수로 변환된다. 역변환한 거리와 실제로 그려지는 행의 경계가 정확히 일치하지 않아 충돌 시점을 놓치는 경우가 생겼다.

### 최종 해결

장애물의 이전 프레임 화면 행과 현재 프레임 화면 행을 모두 계산하여, 그 사이에 펭귄의 발 행이 포함되는지 검사했다.

```cpp
const int previousObstacleScreenY =
    DistanceToScreenY(obstacle->GetPreviousDistance());

const bool crossedPlayerRow =
    previousObstacleScreenY <= currentPlayerScreenY
    && obstacleScreenY >= currentPlayerScreenY;
```

이 방식은 장애물이 발 행에 도착하기 전에는 충돌하지 않으며, 속도가 빨라 한 프레임에 여러 행을 이동해도 발 행을 통과했는지 확인할 수 있다.

### 추가 조정: 몸통 끝이 가시를 통과한 문제

미끄러지는 펭귄의 오른쪽 몸통 문자 `)`가 가시에 닿았는데도 충돌하지 않는 경우가 있었다. 플레이어의 논리적 가로 반폭 `0.10`이 ASCII 몸통보다 좁고, 두 충돌 범위가 정확히 맞닿은 경우를 `<` 비교에서 제외한 것이 원인이었다.

플레이어 반폭을 `0.13`으로 조정하고 경계 접촉도 충돌에 포함했다.

```cpp
float horizontalHalfWidth = 0.13f;

const bool horizontalOverlap =
    std::abs(playerX - obstacleX) <= combinedHalfWidth;
```

긴 왼쪽 `_` 부분은 미끄러짐을 표현하는 시각 요소로 보고 히트박스에 모두 포함하지 않았다. 몸 중앙과 오른쪽 몸통이 닿으면 충돌하지만, 꼬리 끝을 살짝 스치는 경우는 허용한다.

### 추가 조정: 화면에서 `^`가 몸통에 닿아도 통과한 문제

정규화된 도로 좌표의 반폭만 넓혀도 도로 원근 폭과 정수 좌표 변환에 따라 실제 ASCII 문자 위치와 약간의 차이가 남았다. 그 결과 가시 `^` 하나가 오른쪽 몸통 `)`에 스쳐 보였지만 논리 범위가 겹치지 않는 경우가 있었다.

LowSpike는 충돌 행에서 실제 화면 X 구간을 계산하도록 변경했다.

```cpp
// 미끄러지는 펭귄의 중앙 몸통: x - 2 ~ x + 4
// 가까운 LowSpike "^^^^":     x - 2 ~ x + 1
const bool spikeScreenOverlap =
    playerCollisionX - 2 <= obstacleCollisionX + 1
    && playerCollisionX + 4 >= obstacleCollisionX - 2;
```

이제 가시 문자 한 칸이라도 중앙 몸통과 겹치면 충돌한다. 왼쪽의 긴 미끄러짐 잔상 `_`는 계속 판정에서 제외한다.

### 충돌 범위 해석

- 펭귄 ASCII 전체를 문자 단위로 충돌 처리하지 않는다.
- 장애물과 펭귄의 논리적인 가로 범위가 겹칠 때만 충돌한다.
- 날개나 ASCII 여백만 스친 경우에는 통과할 수 있다.
- 몸 중앙이나 발을 관통해도 통과한다면 가로 충돌 폭을 다시 조정해야 한다.

---

## BrokenBridge 시각적 위치와 판정 위치 동기화

### 문제 현상

- 화면에서는 다리의 앞쪽 균열선이 이미 펭귄 아래로 지나갔는데 뒤늦게 추락 판정이 발생했다.
- 충분히 점프한 것처럼 보이는데도 `FELL THROUGH BROKEN BRIDGE`가 표시됐다.

### 원인

가까운 BrokenBridge는 장애물 중심을 기준으로 `y - 2`, `y - 1`, `y`의 세 행에 그려진다.

```cpp
drawGapRow(y - 2, "/\\", Craft::Color::BrightWhite);
drawGapRow(y - 1, "~-", Craft::Color::Blue);
drawGapRow(y, "\\/", Craft::Color::BrightWhite);
```

하지만 충돌은 `obstacleScreenY - 1`을 기준으로 검사했다. 실제 앞쪽 균열선인 `y - 2`보다 한 줄 늦은 위치에서 점프 높이를 판단하고 있었다.

### 해결 방법

다리의 접촉 위치를 실제 앞쪽 균열선과 같은 `y - 2`로 맞췄다.

```cpp
const int contactScreenY = isBrokenBridge
    ? obstacleScreenY - 2
    : obstacleScreenY;
```

또한 이전 프레임과 현재 프레임의 균열선 위치를 비교하여, 균열선이 펭귄 발 행을 통과한 순간에만 점프 높이를 검사하도록 변경했다.

```cpp
const bool crossedBridgeEntry =
    previousContactScreenY <= currentPlayerScreenY
    && contactScreenY >= currentPlayerScreenY;
```

통과 조건은 논리적 점프 높이 `0.6` 이상으로 유지했다.

```cpp
const bool clearedBrokenBridge = isBrokenBridge
    && player->GetJumpHeight() >= 0.6f;
```

판정 위치와 난이도 수치를 동시에 바꾸지 않음으로써, 문제가 충돌 시점 때문인지 점프 요구 높이 때문인지 따로 검증할 수 있도록 했다.

### 검증 결과

- Debug x64 빌드 성공
- LowSpike는 발 행을 통과하는 프레임에 판정
- BrokenBridge는 앞쪽 균열선이 발 행을 통과하는 프레임에 판정
- 빠른 속도에서도 이전·현재 행 사이를 검사하여 충돌 누락 방지

### 남은 플레이 테스트

- 다리 균열선과 실제 추락 시점이 화면상 일치하는지 확인
- 후반 속도에서 점프 높이 `0.6`의 허용 시간이 충분한지 확인
- LowSpike가 몸 중앙을 통과할 때 정상적으로 충돌하는지 확인
- 장애물을 옆으로 피했을 때 불필요한 충돌이 발생하지 않는지 확인

### 추가 조정: 판정이 한 줄 빨랐던 문제

앞쪽 흰 균열선인 `y - 2`는 다리가 깨졌다는 것을 보여주는 장식선이고, 실제로 빠지는 공간은 파란 물이 표시되는 `y - 1`이었다. 흰 선을 접촉 기준으로 사용하자 펭귄이 아직 틈에 진입하지 않았는데도 추락 판정이 발생했다.

따라서 BrokenBridge의 접촉 기준을 실제 물 구간과 같은 `y - 1`로 변경했다.

```cpp
const int contactScreenY = isBrokenBridge
    ? obstacleScreenY - 1
    : obstacleScreenY;
```

이전·현재 프레임 사이의 행 통과 검사는 그대로 유지하므로, 후반 속도에서도 실제 틈의 시작 행을 건너뛴 순간을 놓치지 않는다.

---

## Level 역할 분리

- 최초 타이틀 화면과 입력을 `TitleLevel`로 분리했다.
- `TitleLevel`은 Enter로 `PolarLevel`을 시작하고 Esc로 게임을 종료한다.
- `PolarLevel`은 실제 주행과 플레이 중 일시정지 메뉴만 담당한다.
- 함수 종류별로 하나의 Level 클래스를 여러 cpp 파일에 나누지 않고, 참고 프로젝트처럼 역할이 다른 Level을 별도 클래스로 구성했다.
