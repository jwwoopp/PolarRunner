# PolarRunner 발표용 트러블슈팅 정리 — 2026-08-18

발표 대상 3개 주제. 각 항목은 `증상 → 원인 → 해결 → 결과` 순서로 정리했다.

---

# 1. IceWall 충돌 판정

가장 오래 걸린 문제이며, 원인이 세 겹으로 쌓여 있었다.

## 1-1. 화면에는 여백이 있는데 충돌하는 문제

### 증상

펭귄 몸통 `)`과 IceWall `|` 사이에 눈으로 보이는 빈 칸이 있는데도
`CRASH: ICE WALL`이 발생했다.

### 원인

`Player::Draw()`와 충돌 판정이 **몸통 시작 X를 각자 계산**하고 있었다.
같은 값을 두 곳에서 따로 구했기 때문에 방향 전환 시 두 값이 어긋났다.

- `Draw()`: 방향에 따라 `x - 5`(오른쪽) 또는 `x - 1`(왼쪽)에서 `(_____)` 출력
- 충돌 판정: 방향과 무관한 자체 오프셋 사용

### 해결

몸통 기준을 `Player` 하나로 모으고, `Draw()`와 판정이 같은 함수를 쓰도록 했다.

```cpp
// Player.h
inline int GetBodyLeftOffset() const { return isFacingRight ? -5 : -1; }
static constexpr int BodyWidth = 7;
```

```cpp
// Draw()와 충돌 판정이 동일한 계산을 사용
const int bodyX = x + GetBodyLeftOffset();
```

### 결과

방향을 바꿔도 화면에 그려진 몸통과 판정 범위가 항상 일치한다.

## 1-2. 커브 구간에서만 오충돌하는 문제

### 증상

직선에서는 정상인데 **커브 구간에서만** 화면상 닿지 않은 벽과 충돌했다.

### 원인

pseudo-3D 도로는 **화면 행마다 도로 중심이 다르다**.
`CalculateRoadSlice(depth)`가 행별로 `centerX`를 따로 계산하기 때문이다.

그런데 판정 코드는 벽의 X를 **플레이어의 행**에서 다시 투영하고 있었다.
`Draw()`는 **벽 자신의 행**에서 투영한다. 커브에서는 두 행의 도로 중심이
다르므로, 화면에 보이는 벽과 판정용 벽의 위치가 어긋났다.

### 해결

판정에서도 `Draw()`와 완전히 같은 행·같은 X를 쓰도록 통일했다.

```cpp
// PolarObstacle.cpp — 벽 자신의 행에서 투영 (Draw()와 동일)
const int y = level->DistanceToScreenY(distance);
const int x = level->GetRoadScreenX(horizontalPosition, y);
```

### 결과

**렌더링과 충돌이 같은 좌표 기준을 공유**하게 되어 커브 오충돌이 사라졌다.

## 1-3. 판정 폭 off-by-one

### 증상

미세하게 어긋나는 충돌이 남아 있었다.

### 원인

`Draw()`는 문자열을 `x - size / 2`에서 시작해 `size`칸을 출력한다.
판정은 `x ± halfWidth`로 계산했는데, **짝수 폭에서 오른쪽이 한 칸 더 넓어진다.**
가까운 IceWall 몸통은 8칸이므로 9칸으로 판정되어 보이지 않는 충돌 칸이 생겼다.

### 해결

문자열 길이 그대로 끝 좌표를 구했다.

```cpp
bounds.left  = x - bodyWidth / 2;
bounds.right = bounds.left + bodyWidth - 1;   // 문자열 길이와 동일
bounds.top    = y - bodyRowSpan;              // 지붕 제외, 몸통 행만
bounds.bottom = y;
```

지붕(뾰족한 장식)은 위로 갈수록 좁아지므로 판정에서 제외하고,
일정 폭으로 그려지는 몸통 행만 `ScreenBounds`로 사용한다.

### 결과

화면에 출력된 칸 수와 판정 칸 수가 정확히 같아졌다.
외곽선 `|`도 실제 벽으로 취급해, 닿으면 충돌한다.

## 발표용 한 줄

> 렌더링과 충돌 판정이 좌표를 각각 계산하던 구조를 하나로 통일해 해결했다.

## 배운 점

- 같은 값을 두 곳에서 계산하면 반드시 어긋난다. 계산은 한 곳에 두고 공유한다.
- 추측으로 마진을 넓히지 않고, 로그에 남긴 `playerX` / `wallX` 실측값으로
  원인을 특정했다.

---

# 2. 맵 원근감

## 2-1. 거리 → 화면 행 변환을 비선형으로

### 문제

거리를 화면 Y에 **선형**으로 매핑하면 원근감이 나지 않는다.
멀리 있는 물체가 너무 빨리 다가오고, 가까운 물체는 느리게 느껴진다.

### 해결

거리를 정규화한 뒤 지수를 적용해, 먼 행은 촘촘하게 가까운 행은 넓게 배치했다.

```cpp
int PolarLevel::DistanceToScreenY(float distance) const
{
    const float normalized = std::clamp(1.0f - distance / viewDistance, 0.0f, 1.0f);
    const float perspective = std::pow(normalized, 1.55f);
    const int roadTopY = horizonY + 1;
    const int roadBottomY = screenHeight - 1;
    return roadTopY + static_cast<int>(perspective * (roadBottomY - roadTopY));
}
```

지수 `1.55`가 원근 강도를 결정한다. 이 값이 커지면 지평선 쪽이 더 압축되어
가까워질 때 급격히 커지는 느낌이 강해진다.

## 2-2. 역변환으로 충돌을 판정하려다 실패

### 증상

장애물이 플레이어를 그대로 통과하거나, 충돌 시점을 놓치는 경우가 생겼다.

### 원인

화면 행을 거리로 되돌려(`ScreenYToDistance`) 비교하려 했다.

```cpp
// 실패한 접근
const float playerCollisionDistance = ScreenYToDistance(currentPlayerScreenY);
const bool crossed =
    obstacle->GetPreviousDistance() >= playerCollisionDistance
    && obstacle->GetDistance() <= playerCollisionDistance;
```

문제는 `DistanceToScreenY`가 마지막에 `static_cast<int>`로 **정수 행으로
양자화**한다는 점이다. 역변환한 실수 거리와 실제로 그려지는 행의 경계가
정확히 일치하지 않아, 그 틈에서 판정이 누락됐다.

### 해결

거리로 되돌리지 않고, **화면 행 자체를 비교**했다.
이전 프레임 행과 현재 프레임 행 사이에 플레이어 행이 포함되는지 검사한다.

```cpp
const int previousObstacleScreenY =
    DistanceToScreenY(obstacle->GetPreviousDistance());

const bool crossedPlayerRow =
    previousObstacleScreenY <= currentPlayerScreenY
    && obstacleScreenY >= currentPlayerScreenY;
```

### 결과

- 정수 양자화 오차의 영향을 받지 않는다.
- 속도가 빨라 한 프레임에 여러 행을 건너뛰어도 통과 여부를 놓치지 않는다.
  (swept 방식과 같은 원리)

## 2-3. 커브가 지평선에서 접히는 문제

### 증상

커브 오프셋을 모든 행에 그대로 적용하니, 도로 폭이 좁은 지평선 근처에서
좌우로 크게 흔들려 도로가 꺾이거나 접히는 것처럼 보였다.

### 해결

깊이에 smoothstep을 적용해 **먼 행은 거의 움직이지 않고, 가까운 행일수록
크게 휘도록** 했다. 커브가 지평선에서 튀지 않고 자연스럽게 자라나온다.

```cpp
const float curvePerspective = depth * depth * (3.0f - 2.0f * depth);
const float maximumCurveOffset = screenWidth * 0.18f;
slice.centerX = screenWidth / 2 + static_cast<int>(
    slice.centerOffset * curvePerspective * maximumCurveOffset);
```

도로가 화면 밖으로 밀려나지 않도록 가시 폭도 함께 제한한다.

```cpp
const int maximumVisibleHalfWidth = (std::max)(2,
    (std::min)(slice.centerX - 1, screenWidth - slice.centerX - 2));
```

## 2-4. 지형별 도로 폭 구분

지형마다 최소 폭 배율을 달리 두어 같은 원근 계산으로 난이도 차이를 만들었다.

```cpp
float minimumWidthScale = 0.72f;                 // 기본
if (slice.terrain == TerrainType::Canyon)        { minimumWidthScale = 0.45f; }
else if (slice.terrain == TerrainType::NarrowIcePath) { minimumWidthScale = 0.28f; }
```

또한 코스 웨이포인트(`roadSlices`) 사이를 smoothstep으로 보간해
커브와 폭이 급격히 바뀌지 않게 했다.

```cpp
amount = amount * amount * (3.0f - 2.0f * amount);
```

## 발표용 한 줄

> 거리를 비선형으로 화면 행에 매핑해 원근감을 만들고, 커브는 깊이에 따라
> 점진적으로 적용해 지평선에서 튀지 않게 했다.

## 배운 점

- 원근 투영은 정수 행으로 양자화되므로, 판정을 실수 거리로 역변환하면 어긋난다.
  **판정도 렌더링과 같은 단위(행)로 하는 것이 안전하다.**
- 화면 좌표는 행마다 도로 중심이 다르다. 이 사실이 IceWall 오충돌의 근본
  원인이기도 했다. (1-2 항목과 연결)

---

# 3. 슈팅 요소

## 3-1. 별 → SHOT 자원 구조

별 5개를 모으면 SHOT 1발을 얻는다. 달리기만 하던 게임에 전투를 넣기 위한 장치다.

```cpp
static constexpr int RequiredStarCount = 5;
```

```cpp
if (collectedStarCount >= RequiredStarCount)
{
    collectedStarCount -= RequiredStarCount;
    ++nonShotCount;      // 발사 가능 횟수
}
```

발사 제약도 두었다. SHOT이 없거나 점프 중에는 쏠 수 없다.

```cpp
if (!player || nonShotCount <= 0 || player->IsJumping()) { return; }
```

## 3-2. 옆 레인의 별이 스치듯 먹히는 문제

### 증상

플레이어가 별과 다른 레인에 있는데도 지나가면서 수집됐다.

### 원인

수집 판정을 **화면 픽셀 폭**으로 비교했다.

```cpp
// 기존
const int playerHalfWidth = player->IsJumping() ? 3 : 5;
const bool horizontalOverlap =
    std::abs(playerScreenX - starScreenX) <= playerHalfWidth;
```

원근 때문에 가까운 행에서는 레인 간 픽셀 거리가 좁아진다.
그래서 옆 레인의 별도 `±5` 안에 들어와 버렸다.

### 해결

화면 픽셀이 아니라 **정규화된 레인 좌표**로 비교했다.

```cpp
constexpr float laneMatchTolerance = 0.25f;
const bool horizontalOverlap =
    std::abs(player->GetHorizontalPosition()
        - star->GetHorizontalPosition()) < laneMatchTolerance;
```

점프 중에는 아예 수집하지 않도록 했다. 점프로 화면상 별과 겹치는 것은
실제로 같은 위치에 있는 것이 아니기 때문이다.

### 결과

같은 레인에 있을 때만 수집된다. 원근 배율과 무관하게 동작한다.

## 3-3. 탄환 이미지가 커질 때 경로가 어긋나는 문제

### 증상

`PlayerBullet`(그물)이 날아가면서 이미지가 커지는데, 커질 때마다 비행선이
한쪽으로 밀렸다.

### 원인

`Actor`의 위치는 이미지의 **왼쪽 끝**이다. 이미지가 `*` → `<>` → `<#>`로
길어지면 왼쪽 끝 기준이 유지되어 중심이 오른쪽으로 이동한다.

### 해결

이미지 폭의 절반만큼 보정해 **중심이 경로를 따르도록** 했다.

```cpp
if (traveledDistance >= 12.0f)     { ChangeImage("<#>"); }
else if (traveledDistance >= 5.0f) { ChangeImage("<>"); }

newPosition.x = static_cast<int>(xPosition) - GetWidth() / 2;
```

## 3-4. 스폰 직후 오판정되는 Enemy 탄환 (엔진 버그)

### 증상

해안에서 **적이 접근하지도 않았는데** `CRASH: HIT BY ENEMY BULLET`이 발생했다.
왼쪽 스폰에서는 드물고, **오른쪽 스폰에서 두드러졌다.**

### 원인

`Craft::Actor` 생성자가 `previousPosition`을 초기화하지 않아 기본값 `(0, 0)`
으로 남아 있었다.

충돌 판정은 탄환의 이전 위치와 현재 위치를 잇는 **swept 선분**으로 검사한다.
스폰 직후 첫 프레임에는 `previousPosition`이 화면 왼쪽 위 `(0, 0)`,
`position`은 실제 스폰 좌표이므로 **그 사이의 거대한 선분이 화면을 가로질렀다.**
화면 오른쪽에서 스폰될수록 이 선분이 플레이어를 지날 확률이 높아 증상이
한쪽으로 치우쳤다.

### 해결

`Actor` 생성자 초기화 리스트에 `previousPosition(position)`을 추가했다.

```cpp
Actor::Actor(const std::string& image, const Vector2& position, Color color)
    : image(image), position(position), previousPosition(position),
      color(color), width(static_cast<int>(image.size()))
{
}
```

### 결과

스폰 첫 프레임에는 이전 위치와 현재 위치가 같아 오판정이 사라졌다.
같은 `previousPosition`을 `CollisionSystem::Test()`도 사용하므로,
`PlayerBullet`–`Enemy` 충돌 등 다른 판정에도 잠재적으로 영향이 있던 버그였다.

### 발표 포인트

증상이 **좌우 비대칭**이라는 점이 원인 추적의 단서였다.
게임 코드가 아니라 엔진 생성자에 원인이 있었고, `PlaytestLog.log`에서
발사 로그와 CRASH 로그 사이에 실제 탄환 이동 시간만큼의 간격이 있는지
확인해 수정을 검증했다.

## 3-5. 좌우 대칭 스폰 일반화

### 문제

해안 Enemy가 항상 화면 왼쪽에서만 스폰됐다. 접근 위치, 정차 위치,
탄환 발사 방향이 전부 왼쪽 스폰을 전제로 하드코딩되어 있었다.

### 해결

`EnemySide`를 도입해 50/50 랜덤으로 정하고, 관련 계산을 모두 좌우 대칭으로
일반화했다.

```cpp
const EnemySide spawnSide = ... ? EnemySide::Left : EnemySide::Right;
coastEnemyScreenX = spawnSide == EnemySide::Left
    ? screenWidth * 0.18f : screenWidth * 0.82f;
```

`Enemy::Draw()`는 선체 ASCII를 좌우 반전해 포문이 항상 도로 쪽을 향하게 했다.

```cpp
const bool facingLeft = side == EnemySide::Right;
Craft::Renderer::Get().Submit(facingLeft ? "<==_O_|" : "|_O_==>", ...);
```

`EnemyBullet`은 왼쪽으로 발사될 때도 정리되도록 양쪽 경계를 모두 검사한다.

```cpp
if (xPosition <= 0.0f || xPosition >= Engine::Get().GetWidth() - 1)
{
    Destroy();
}
```

또한 배가 **바다가 그려진 쪽에서만** 나오도록, 렌더링과 스폰이
`coastOceanOnRight`를 함께 참조하게 했다.

## 3-6. 탄환 터널링 방지

탄환은 프레임당 여러 칸 이동하므로, 현재 위치만 비교하면 플레이어를
지나쳐버릴 수 있다. 이전 위치와 현재 위치의 X 범위로 검사한다.

```cpp
const int bulletLeft  = (std::min)(current.x, previous.x);
const int bulletRight = (std::max)(current.x, previous.x);
const bool overlapsX = bulletRight >= playerLeft && bulletLeft <= playerRight;
```

## 3-7. 명중 판정은 RTTI로

`PlayerBullet`은 부딪힌 상대가 `Enemy`인지 런타임에 확인한다.

```cpp
void PlayerBullet::OnCollision(const std::shared_ptr<Craft::Actor>& other)
{
    if (std::dynamic_pointer_cast<Enemy>(other))
    {
        other->Destroy();
        Destroy();
    }
}
```

엔진의 `CollisionSystem`이 충돌을 감지하고, 타입 판별은 게임 코드가 담당한다.

## 발표용 한 줄

> 별을 모아 SHOT을 얻는 자원 구조로 러너에 전투를 붙였고, 스폰 첫 프레임의
> swept 충돌 오판정을 엔진 생성자에서 찾아 수정했다.

## 배운 점

- 원근이 적용된 화면에서는 픽셀 거리가 위치마다 다른 의미를 가진다.
  게임 규칙(레인 일치)은 정규화 좌표로 판정해야 한다.
- swept 충돌은 이전 위치가 유효하다는 전제 위에서만 동작한다.
  전제가 깨지면 판정이 무작위로 보인다.
- 증상의 비대칭성(오른쪽에서만 심함)은 원인을 좁히는 강력한 단서다.

---

# 남은 개선 과제

발표에서 질문받을 경우 대비.

- `CheckEnemyBulletCollisions()`의 플레이어 몸통 범위가 아직
  `IsJumping() ? 3 : 5`로 하드코딩되어 있다. IceWall에서 통일한
  `GetBodyLeftOffset()` / `BodyWidth`를 여기에도 적용해야 한다.
- `enemyBullets` 벡터가 비워지지 않아 런 내내 누적된다.
  만료된 탄환을 매 프레임 정리해야 한다.
- Enemy 등장 구간이 지형 웨이포인트와 별개 배열로 관리되어 손으로 맞춰야 한다.
  지형 구간에서 Enemy 허용 여부를 도출하는 구조로 바꾸는 것이 바람직하다.
- 지형 dispatch가 `switch`로 고정되어 있어, 지형 추가 시 여러 곳을 함께
  수정해야 한다.
