# PolarRunner IceWall 충돌 트러블슈팅

## 문제

점프 중인 펭귄과 IceWall 사이에 화면상 여백이 있는데도 `CRASH: ICE WALL`이 발생했다.

## 원인

점프 자세와 미끄러지는 자세의 ASCII 몸통 위치가 서로 다른데, 충돌 판정에서는 항상 미끄러지는 자세의 방향별 오프셋을 사용하고 있었다.

- 점프 몸통 `/( _ )\`: 방향과 관계없이 중심 기준 `x - 3 ~ x + 3`
- 기존 판정: 미끄러지는 자세 기준 `x - 5` 또는 `x - 1`에서 시작

특히 펭귄이 왼쪽을 바라보며 점프할 때 충돌 범위가 실제 그림보다 오른쪽으로 2칸 밀렸다. 세로 판정도 점프로 올라간 몸통이 아니라 지상 기준 Y 좌표를 사용하고 있었다.

## 해결

플레이어의 현재 자세에 따라 IceWall과 비교할 몸통 범위를 다르게 계산했다.

```cpp
const int playerBodyLeft = playerCollisionX
    + (player->IsJumping() ? -3 : player->GetBodyLeftOffset());

const int playerVisualBaseY = currentPlayerScreenY
    - player->GetJumpScreenOffset();
const int playerBodyTop = player->IsJumping()
    ? playerVisualBaseY - 3 : playerVisualBaseY;
const int playerBodyBottom = player->IsJumping()
    ? playerVisualBaseY - 1 : playerVisualBaseY;
```

- 점프 중: 실제 점프 ASCII 몸통의 X/Y 범위를 사용
- 지상: 기존 미끄러지는 몸통 범위를 유지
- 부리와 발은 몸통 충돌 범위에서 제외

## 결과

화면에 그려진 펭귄 몸통과 IceWall 몸통이 실제로 겹칠 때만 충돌하도록 판정과 표현을 일치시켰다.

## 검증

- Debug x64 컴파일 성공
- 경고 0개, 오류 0개
- 실제 실행에서 점프 중 근접 통과와 몸통 접촉 상황을 추가 확인할 필요가 있다.

---

## 추가 문제: 멀리 있는 IceWall과 조기 충돌

### 증상

점프한 펭귄과 멀리 있는 IceWall이 화면상 비슷한 Y 위치에 표시되자, 장애물이 실제 플레이어 진행선에 도달하기 전에 충돌했다.

### 원인

pseudo-3D 화면의 Y 좌표에는 서로 다른 두 의미가 섞여 있다.

- 장애물의 화면 Y: 진행 거리(`distance`)를 원근 투영한 결과
- 플레이어의 화면 Y: 진행 위치에 점프 연출 오프셋을 추가한 결과

기존 판정은 두 화면 Y 범위가 겹치는지를 검사했다. 그 결과 점프로 위에 그려진 플레이어를 실제로 전진한 것으로 잘못 해석했고, 높이가 큰 IceWall ASCII와 멀리서 겹쳐도 충돌했다.

### 해결

IceWall의 충돌 시점을 화면 도형의 세로 AABB가 아니라 장애물이 플레이어의 진행 거리 선상을 통과하는 순간으로 변경했다.

```cpp
const bool crossedPlayerRow =
    previousObstacleScreenY <= currentPlayerScreenY
    && obstacleScreenY >= currentPlayerScreenY;

const bool reachesPlayer = isBrokenBridge
    ? crossedBridgeEntry : crossedPlayerRow;
```

충돌 판정 순서는 다음과 같다.

1. 장애물이 플레이어 진행선에 도달했는지 확인
2. 해당 순간에 플레이어 몸통과 장애물의 가로 범위 확인
3. 장애물별 점프 회피 조건 확인
4. 모든 충돌 조건이 만족되면 Crash 처리

IceWall이 진행선을 지난 뒤에는 검사 완료로 표시하여 동일 장애물을 반복 판정하지 않는다.

### 결과

- 점프 연출로 플레이어가 위쪽에 그려져도 실제 진행 거리는 바뀌지 않는다.
- 멀리 있는 IceWall의 큰 ASCII와 화면에서 겹치는 것만으로 충돌하지 않는다.
- 플레이어와 IceWall이 같은 진행 거리 선상에 도달했을 때만 좌우 충돌을 검사한다.

### 검증

- Debug x64 컴파일 성공
- 경고 0개, 오류 0개
- 실제 플레이에서 조기 충돌과 정상 몸통 충돌을 다시 확인할 필요가 있다.
