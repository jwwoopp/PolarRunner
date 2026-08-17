# PolarRunner 주요 트러블슈팅 정리 (발표용)

> 콘솔 ASCII 러너 게임 PolarRunner에서 개발 기간 중 가장 오래 붙잡았던
> 세 가지 문제와 그 해결 과정을 정리한 문서입니다.
>
> 1. IceWall 충돌 판정
> 2. 맵 원근감(pseudo-3D) 표현
> 3. 슈팅 요소(별 → SHOT → Enemy 전투)

---

## 0. 한 장 요약

| 주제 | 핵심 증상 | 근본 원인 | 해결의 핵심 |
| --- | --- | --- | --- |
| IceWall 충돌 | 안 닿았는데 CRASH / 닿았는데 통과 | **그리는 좌표와 판정하는 좌표를 각자 계산** | `Draw()`와 판정이 같은 함수·같은 기준을 공유 |
| 맵 원근감 | 커브에서 도로가 접히고, 지형이 순간 교체 | 화면 Y 한 값에 **거리 투영 + 연출**이 섞임 | 행마다 자기 거리로 샘플링, 화면은 코스 데이터만 신뢰 |
| 슈팅 요소 | 스폰하자마자 피격 CRASH, 배가 땅 위에 뜸 | 이전 프레임 상태 미초기화 + 스폰 규칙 하드코딩 | `previousPosition` 초기화, 지형 데이터 기준으로 스폰 |

전체를 관통하는 한 문장:

> **"보이는 것(Draw)과 판정하는 것(Collision)이 서로 다른 계산을 하면 반드시 어긋난다."**

---

## 1. IceWall 충돌 판정

### 1-1. 증상

플레이 중 가장 자주 나온 컴플레인이 IceWall이었습니다. 증상이 한 가지가 아니라
정반대 두 가지가 번갈아 나타났습니다.

- 화면에서는 벽과 펭귄 사이에 **빈 칸이 보이는데** `CRASH: ICE WALL`
- 반대로 벽 안쪽으로 **몸통이 들어갔는데도** 그냥 통과
- 커브 구간에서 특히 오충돌이 잦음
- 점프 중, 특히 **왼쪽을 보며 점프**할 때 억울한 충돌이 집중됨
- 아직 한참 멀리 있는 벽인데 점프하면 **조기 충돌**

### 1-2. 원인 — 좌표계가 두 벌이었다

IceWall은 거리(`closeness`)에 따라 **화면에 그려지는 폭 자체가 달라지는** 장애물입니다.

```cpp
// PolarObstacle::Draw() — 거리에 따라 ASCII가 통째로 바뀐다
if (closeness > 0.72f) { /* "|######|"  8칸 */ }
else if (closeness > 0.30f) { /* "|####|"  6칸 */ }
else { /* "/\"  2~3칸 */ }
```

그런데 초기 충돌 판정은 **정규화된 도로 좌표(월드 lane 좌표)** 의 반폭만 비교했습니다.

```cpp
const bool horizontalOverlap =
    std::abs(playerX - obstacleX) < playerHalfWidth + obstacleHalfWidth;
```

즉 `Draw()`는 "거리에 따라 2~8칸"으로 그리는데 판정은 "항상 고정 반폭"으로 하고
있었습니다. 눈에 보이는 벽과 판정용 벽이 애초에 다른 물체였던 셈입니다.

여기에 원인이 네 겹으로 더 쌓여 있었습니다.

1. **짝수 폭 off-by-one**
   `Draw()`는 `x - row.size() / 2`에서 문자열을 시작하는데, 판정은 `x ± halfWidth`로
   계산했습니다. 8칸/6칸 같은 짝수 폭에서 **오른쪽으로 한 칸 넓은 보이지 않는 충돌 칸**이
   생겼습니다.
2. **커브에서의 재투영**
   판정 코드가 벽의 X를 *플레이어의 화면 행*에서 다시 투영했습니다. 커브에서는 행마다
   도로 중심이 다르므로, 화면의 벽과 판정용 벽이 좌우로 어긋났습니다.
3. **자세별 ASCII 차이**
   미끄러지는 몸통 `(_____)`은 바라보는 방향에 따라 시작 오프셋이 `-5`/`-1`인데,
   점프 몸통 `/( _ )\`은 방향과 무관하게 `-3`에서 시작합니다. 판정이 항상 미끄러짐
   기준을 쓰는 바람에 **왼쪽을 보며 점프하면 히트박스가 2칸 밀렸습니다.**
4. **화면 Y의 의미 혼재**
   장애물의 화면 Y는 *거리를 원근 투영한 결과*이고, 플레이어의 화면 Y는
   *진행 위치 + 점프 연출 오프셋*입니다. 두 화면 Y의 AABB를 겹쳐 보면, 점프로 위에
   그려진 펭귄이 "전진한 것"으로 잘못 해석되어 **키가 큰 원경 IceWall과 조기 충돌**했습니다.

### 1-3. 해결

**(1) 판정용 화면 범위를 Draw와 같은 규칙으로 계산하는 단일 함수를 만들었습니다.**

```cpp
// PolarObstacle::GetIceWallScreenBounds()
const int y = level->DistanceToScreenY(distance);
// Draw()와 완전히 같은 행에서 X를 구한다 (커브 어긋남 제거)
const int x = level->GetRoadScreenX(horizontalPosition, y);
const float closeness = 1.0f - distance / level->GetViewDistance();

int bodyWidth = visualVariant == 0 ? 2 : 3;   // 지붕 제외, 몸통만
int bodyRowSpan = 0;
if (closeness > 0.72f)      { bodyWidth = 8; bodyRowSpan = 2; }
else if (closeness > 0.30f) { bodyWidth = 6; bodyRowSpan = 1; }

// 문자열 길이 그대로 끝 좌표를 구해 off-by-one 제거
bounds.left  = x - bodyWidth / 2;
bounds.right = bounds.left + bodyWidth - 1;
```

- 뾰족한 **지붕 행은 제외**하고 실제로 벽인 몸통 행만 판정에 사용
- 외곽선 `|`도 실제 벽으로 취급 (괄호가 `|`에 닿는 순간 Crash)

**(2) 플레이어 몸통 범위도 `Draw()`와 공유하도록 바꿨습니다.**

```cpp
// Player.h — Draw()와 판정이 같은 값을 쓴다
inline int GetBodyLeftOffset() const { return isFacingRight ? -5 : -1; }
static constexpr int BodyWidth = 7;   // "(_____)"

// 판정부 — 점프 자세는 방향과 무관하게 -3에서 시작
const int playerBodyLeft = playerCollisionX
    + (player->IsJumping() ? -3 : player->GetBodyLeftOffset());
const int playerBodyRight = playerBodyLeft + (Player::BodyWidth - 1);
```

**(3) 충돌 "시점"은 화면 겹침이 아니라 진행선 통과로 판정했습니다.**

```cpp
// 장애물의 이전 프레임 행 ~ 현재 프레임 행 사이에 플레이어 발 행이 있는가
const bool crossedPlayerRow =
    previousObstacleScreenY <= currentPlayerScreenY
    && obstacleScreenY >= currentPlayerScreenY;
```

이 스윕(swept) 행 검사는 속도가 빨라 한 프레임에 여러 행을 건너뛰어도 충돌을
놓치지 않습니다. 최종 판정 순서는 다음과 같습니다.

```text
1) 장애물이 플레이어 진행선을 통과했는가   (crossedPlayerRow)
2) 그 순간 몸통 X 범위가 겹치는가          (iceWallScreenOverlap)
3) 장애물별 회피 조건을 만족했는가          (점프 / 레인 회피)
4) 모두 참이면 Crash, 지나간 장애물은 MarkChecked()로 재판정 제외
```

**(4) 판정 근거를 로그로 남겨 재현 가능하게 만들었습니다.**

```cpp
crashEntry << " playerX=[" << playerBodyLeft << "," << playerBodyRight
           << "] wallX=[" << iceWallBounds.left << "," << iceWallBounds.right << "]";
```

"억울하다"는 체감을 숫자로 확인할 수 있게 되어, 이후 조정은 추측이 아니라 로그를
보고 진행했습니다.

### 1-4. 결과 / 발표 포인트

- 화면에 그려진 벽 몸통과 펭귄 몸통이 **실제로 겹칠 때만** 충돌
- 부리·꼬리 잔상 같은 연출 문자는 히트박스에서 제외 → 아슬아슬한 통과가 "실력"으로 느껴짐
- **배운 것**: 렌더링과 판정이 같은 수치를 각자 계산하면 언젠가 반드시 갈라진다.
  둘 중 하나를 고칠 때 다른 하나가 따라오도록 **공유 함수(단일 소스)** 로 묶어야 한다.

---

## 2. 맵 원근감 (Pseudo-3D)

### 2-1. 목표

콘솔 80×30 문자 화면에서 "달리는 느낌"을 만들어야 했습니다. 실제 3D는 없고,
가진 것은 **거리(distance) 하나뿐**입니다. 이걸 화면 행(row)으로 바꾸는 것이 전부입니다.

### 2-2. 핵심 투영식

```cpp
int PolarLevel::DistanceToScreenY(float distance) const
{
    const float normalized = std::clamp(1.0f - distance / viewDistance, 0.0f, 1.0f);
    const float perspective = std::pow(normalized, 1.55f);   // 비선형 압축
    return roadTopY + static_cast<int>(perspective * (roadBottomY - roadTopY));
}
```

- 선형 매핑이면 먼 거리와 가까운 거리가 같은 속도로 다가와 **원근감이 없습니다.**
- 지수 `1.55`를 넣어 **지평선 근처는 촘촘하게, 발밑은 성기게** 배치했습니다.
  멀리서 천천히 나타나다 가까워질수록 급격히 커지는 러너 특유의 감각이 여기서 나옵니다.
- 도로 폭도 같은 깊이 값으로 스케일합니다.

```cpp
const int nearHalfWidth = static_cast<int>(screenWidth * 0.38f);
const int perspectiveHalfWidth = 2 + static_cast<int>(depth * (nearHalfWidth - 2));
```

지평선에서는 반폭 2칸, 발밑에서는 화면의 38%까지 벌어집니다.

### 2-3. 문제 ①: 커브에서 도로가 "접혔다"

초기 커브는 **전역 `curveStrength` 하나**를 모든 행에 적용하고, 오프셋을 지평선에서
최대가 되도록 줬습니다. 그 결과 폭이 2칸밖에 안 되는 지평선 행이 좌우로 크게 흔들려
도로가 꺾이거나 접히는 것처럼 보였고, 실제 코스 데이터와 화면도 따로 놀았습니다.

**해결** — 행마다 자기 거리를 역산해 **그 지점의 코스 데이터를 직접 샘플링**하고,
먼 행의 횡이동은 smoothstep으로 억제했습니다.

```cpp
PolarLevel::RoadSlice PolarLevel::CalculateRoadSlice(float depth) const
{
    // 이 화면 행이 실제로 몇 m 앞인지 역산해서 코스 프로필을 가져온다
    const float normalizedDistance = std::pow(depth, 1.0f / 1.55f);
    const float forwardDistance = (1.0f - normalizedDistance) * viewDistance;
    RoadSlice slice = GetRoadProfile(traveledDistance + forwardDistance);

    // 먼 곳의 횡이동을 억제해 커브가 "지평선에서 자라나오도록" 만든다
    const float curvePerspective = depth * depth * (3.0f - 2.0f * depth);
    slice.centerX = screenWidth / 2
        + static_cast<int>(slice.centerOffset * curvePerspective * maximumCurveOffset);
    ...
}
```

추가로,

- 도로가 화면 밖으로 나가지 않도록 `maximumVisibleHalfWidth`로 클램프
- 코스 슬라이스 사이는 smoothstep(`t*t*(3-2t)`) 보간 → 지형·폭·커브가 **튀지 않고** 전환
- 지형별 최소 폭 배율(협곡 `0.45`, 좁은 얼음길 `0.28`)로 **멀리서도 지형 실루엣이 구분**

### 2-4. 문제 ②: 화면 Y ↔ 거리 왕복이 맞지 않았다

충돌 판정에서 "플레이어 행을 거리로 역변환해 장애물 거리 구간과 비교"하는 접근을
시도했다가 실패했습니다.

```cpp
// 실패한 접근
const float playerCollisionDistance = ScreenYToDistance(currentPlayerScreenY);
const bool crossed = obstacle->GetPreviousDistance() >= playerCollisionDistance
    && obstacle->GetDistance() <= playerCollisionDistance;
```

원근 투영 결과가 **정수 행으로 잘리기 때문에**, 역변환한 거리와 실제로 그려지는 행의
경계가 일치하지 않아 충돌을 통째로 놓쳤습니다.

**교훈**: 정수 화면 좌표는 손실 압축입니다. 왕복 변환에 의존하지 말고
**"행을 통과했는가(swept row test)"** 라는 화면 기준 질문으로 바꿔야 합니다.

### 2-5. 문제 ③: 지형이 순간 교체되는 현상

Enemy 전투 중 렌더링 코드가 **Enemy 생존 여부를 보고 모든 RoadSlice의 지형을
`Coast`로 강제 변경**하고 있었습니다. 코스 데이터와 화면이 서로 다른 진실을 갖게 되어,
Enemy가 사라지는 순간 다른 맵으로 갈아 끼운 것처럼 보였습니다.

**해결**

- 화면 지형은 **항상 `CalculateRoadSlice()`가 반환한 코스 데이터만** 사용
- Enemy 등장 여부는 **해안 구간 안에서만** 판단 (전투 상태가 맵을 바꾸지 않음)
- 격추 직후에는 `coastVisualHoldTimer`(2.5초)로 해안을 유지하되, 이는 렌더 상태일 뿐
  도로 좌표·장애물·충돌 판정은 건드리지 않음
- Enemy가 제거되는 **그 프레임**도 해안으로 유지(`coastEnemyWasActive`) → 1프레임 깜빡임 제거

### 2-6. 그 외 원근 표현 장치

| 장치 | 구현 | 목적 |
| --- | --- | --- |
| 스프라이트 LOD | `closeness > 0.80 / 0.55 / 0.30` 단계별 ASCII 교체 (`^^^^` → `^^` → `^`) | 거리감 표현 |
| 페인터 알고리즘 | `Submit(..., sortingOrder = y + 10)`, 다리 `y + 50`, Enemy `120` | 가까운 것이 위에 |
| 화면 고정 배경 | 하늘·산·지평선은 고정, **도로만** 커브 | 카메라가 고정된 느낌 |
| 지형 렌더 통합 | `DrawRoadEdges()` / `DrawTerrainFill()` 공용 골격으로 6종 지형 통합 | 중복 제거, 규칙 일원화 |
| 플레이어 전후 이동 | `longitudinalScreenOffset`(-8 ~ +4)만큼 판정 행 이동 | 투영 범위를 화면 전체로 확장 |

> ※ 스프라이트 LOD는 원근감을 만드는 장치인 동시에 **1번 IceWall 문제의 근원**이기도
> 했습니다. "거리에 따라 그림이 바뀐다"는 결정이 곧 "거리에 따라 히트박스도 바뀌어야
> 한다"는 요구를 만든 것입니다. 발표에서 두 주제를 연결하기 좋은 지점입니다.

---

## 3. 슈팅 요소

### 3-1. 설계

달리기만 하는 게임에 **자원 → 전투** 루프를 얹었습니다.

```text
별(*) 5개 수집  →  SHOT(@) 1발 획득  →  해안 구간에서 밀렵꾼 배 격추
```

- `F` 키로 발사, 마지막으로 바라본 방향으로 나감
- **점프 중에는 발사 금지** (공중에서 판정 행이 흔들리는 문제 예방)
- SHOT이 0발이면 Enemy가 아예 스폰되지 않음 → 이길 수 없는 전투를 만들지 않음
- Enemy 상태 머신: `ApproachingDepth`(멀리서 접근) → `ClosingSide`(옆으로 붙음) → `Chasing`(사격)
- 추격 중에는 `distance`를 플레이어 머리 줄(`chaseDistance`)에 고정 → 플레이어가 앞뒤로
  움직여도 **같은 화면 줄을 유지**해 탄환 궤적이 예측 가능

### 3-2. 문제 ①: 스폰하자마자 CRASH (가장 어려웠던 버그)

**증상**: 화면 오른쪽에 스폰된 Enemy가 근처에 오지도 않았는데
`CRASH: HIT BY ENEMY BULLET`. 왼쪽 스폰에서는 상대적으로 덜 발생.

**원인**: 엔진의 `Craft::Actor` 생성자가 `previousPosition`을 초기화하지 않아
기본값 `(0, 0)`으로 남아 있었습니다. 충돌 판정은 이전 위치와 현재 위치를 잇는
**스윕 선분**을 쓰기 때문에, 스폰 첫 프레임에는 화면 왼쪽 위 `(0,0)`부터 실제 스폰
좌표까지 **화면을 가로지르는 거대한 판정 선분**이 생겼습니다. 오른쪽에서 스폰될수록
이 선분이 플레이어를 지나갈 확률이 높아 증상이 몰렸던 것입니다.

**해결**: 한 줄이었습니다.

```cpp
Actor::Actor(const std::string& image, const Vector2& position, Color color)
    : image(image), position(position), previousPosition(position),   // ← 추가
      color(color), width(static_cast<int>(image.size()))
{
}
```

**발표 포인트**: 증상은 "적 탄환 버그"였지만 원인은 **엔진 코어의 초기화 누락**이었고,
같은 스윕 판정을 쓰는 PlayerBullet–Enemy 충돌 등 **다른 모든 스폰 프레임 충돌**에도
잠재적으로 영향이 있었습니다. 좌우 편차라는 단서를 따라가 좌표 `(0,0)`을 의심한 것이
해결의 열쇠였습니다.

### 3-3. 문제 ②: 배가 땅 위에 떠 있었다

Enemy 스폰이 **왼쪽 전제로 하드코딩**되어 있었고(접근 위치·정차 위치·탄환 방향 전부),
좌우 스폰을 넣은 뒤에는 배경의 바다 방향과 배의 스폰 방향이 따로 결정되어
**눈밭 위에 배가 떠 있는** 장면이 나왔습니다.

**해결**: 배경과 스폰이 **같은 데이터 소스**를 보게 했습니다.

```cpp
// 배경(DrawCoastRow)에 바다가 그려지는 쪽과 맞춰서 스폰합니다.
const EnemySide spawnSide = coastOceanOnRight[nextCoastEnemyZoneIndex]
    ? EnemySide::Right : EnemySide::Left;
```

부수적으로 `Enemy::Draw()`에서 선체 ASCII를 좌우 반전해 포문이 항상 도로를 향하게 하고,
`EnemyBullet`이 **왼쪽 경계(`x <= 0`)에서도 파괴**되도록 고쳤습니다(원래는 오른쪽만 검사).

### 3-4. 문제 ③: 전투와 미끄러운 지형이 겹쳐 난이도 폭발

Enemy는 격추될 때까지 추격하므로, 전투가 길어지면 다음 `NarrowIcePath`(좁고 미끄러운
얼음길) 구간까지 자연스럽게 겹쳤습니다. 탄환 회피 + 미끄러짐 조작이 동시에 요구되어
사실상 통과 불가 구간이 되었습니다.

렌더링 단계에서 Slip을 임시로 숨기는 방법은 **2번에서 겪은 "화면과 데이터 불일치"를
다시 만드는 선택**이라 배제하고, **스폰 규칙**으로 풀었습니다.

```cpp
// 앞 220m 안에 좁은 얼음길이 있으면 신규 스폰 금지
constexpr float enemySpawnClearanceFromSlip = 220.0f;
const bool hasSafeCombatSpace = !IsNarrowIcePathAhead(enemySpawnClearanceFromSlip);

// 활성 Enemy는 120m 전에 후퇴시키고 기존 탄환도 정리
constexpr float enemyRetreatBeforeSlipDistance = 120.0f;
```

이때 **의도적 후퇴에서는 해안 유지 타이머를 시작하지 않습니다.** 시작하면 다음 Slip
지형을 2.5초 동안 가려버려 플레이어가 준비할 시간을 잃기 때문입니다.
같은 이유로 해안을 벗어나면 남은 Enemy와 탄환을 정리합니다. 그대로 두면 바다가 없는
지형에서 배가 계속 쫓아오고, 한참 뒤에 날아온 탄환에 맞는 일이 생깁니다.

### 3-5. 피격 판정과 난이도 곡선

**자세별 히트박스** — 점프 중에는 좁고 높게, 지상에서는 넓고 낮게.

```cpp
const int playerLeft   = playerX - (player->IsJumping() ? 3 : 5);
const int playerRight  = playerX + (player->IsJumping() ? 3 : 5);
const int playerTop    = playerY - (player->IsJumping() ? 4 : 1);
const int playerBottom = playerY;
```

**탄환은 스윕 X 범위로 검사** — 빠른 탄이 한 프레임에 몸통을 건너뛰는 관통을 방지합니다.

```cpp
const int bulletLeft  = (std::min)(current.x, previous.x);
const int bulletRight = (std::max)(current.x, previous.x);
```

**발사 리듬** — 단순 등간격 연사는 학습이 안 되고 불공정하게 느껴져,
`{느린 예고탄, 빠른 후속탄, 긴 휴식}` 3박자 패턴으로 바꾸고 코스 진행률에 따라
간격 세트를 교체했습니다.

```cpp
constexpr float earlyFireIntervals[]  = { 3.8f, 3.2f, 4.2f };   //      ~ 5000m
constexpr float middleFireIntervals[] = { 3.0f, 2.1f, 3.4f };   // 5000 ~ 10000m
constexpr float lateFireIntervals[]   = { 2.4f, 1.5f, 2.8f };   // 10000m ~
```

**탄환 표현** — 플레이어 탄은 날아간 거리에 따라 그물이 커집니다(`*` → `<>` → `<#>`).
이미지 폭이 커질 때 중심이 밀리지 않도록 위치를 보정했습니다.

```cpp
newPosition.x = static_cast<int>(xPosition) - GetWidth() / 2;
```

---

## 4. 마무리 — 세 문제가 남긴 공통 교훈

1. **화면 좌표와 게임 규칙을 분리한다.**
   점프는 논리 높이 `GetJumpHeight()`(0.0~1.0)로 판정하고, 화면 출력만
   `GetJumpScreenOffset()`으로 변환합니다. ASCII를 고쳐도 난이도가 바뀌지 않습니다.

2. **같은 수치를 두 곳에서 계산하지 않는다.**
   `GetBodyLeftOffset()`, `Player::BodyWidth`, `GetIceWallScreenBounds()`는 모두
   "Draw와 판정이 같은 값을 보게" 만들기 위해 존재하는 API입니다.

3. **정수 화면 좌표는 손실 압축이다.**
   역변환 왕복을 신뢰하지 말고 "이전 프레임과 현재 프레임 사이에 그 행을 지났는가"라는
   스윕 질문으로 바꿉니다.

4. **연출로 데이터를 덮어쓰지 않는다.**
   전투 연출 때문에 지형을 강제로 바꾸면 화면과 코스가 갈라집니다. 렌더는 데이터를
   따라가고, 조절이 필요하면 **스폰 규칙 같은 상위 레벨에서** 해결합니다.

5. **체감을 숫자로 남긴다.**
   `PlaytestLog`에 Crash 시점의 `playerX`/`wallX`, `jumpHeight`/`jumpOffset`,
   Enemy 스폰·사격·격추를 기록해두어, 이후 밸런싱을 추측이 아니라 로그로 진행했습니다.

### 검증

- Visual Studio MSBuild `Debug|x64` 빌드 성공 (경고 0, 오류 0)
- 각 수정은 판정 위치와 난이도 수치를 동시에 바꾸지 않고 분리해 검증
