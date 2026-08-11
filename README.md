# PolarRunner
Wanted5_Project1

## Troubleshooting: 보이지 않는 장애물 충돌

### 증상

- 노란 가시를 좌우로 정상적으로 피했는데 나중에 `CRASH`가 표시됨
- 화면에서 이미 사라진 장애물 근처가 아닌 곳에서도 충돌이 발생함
- 충돌 범위를 줄여도 문제가 완전히 사라지지 않음

### 원인

`DistanceToScreenY()`는 장애물이 플레이어를 지나간 뒤에도 반환값을
플레이어의 화면 Y 좌표로 제한한다. 기존 코드는 화면 Y가 플레이어 아래로
내려갔을 때 장애물을 처리 완료 상태로 만들었기 때문에 해당 조건이 성립하지
않았다.

그 결과 지나간 장애물이 화면에서는 사라져도 충돌 검사 목록에 남았고,
플레이어가 나중에 그 장애물의 가로 위치로 이동하면 보이지 않는 충돌이
발생했다.

### 수정

- 장애물 통과 여부를 화면 Y가 아닌 월드 거리로 판정
- `distance < -2.0f`인 장애물은 `MarkChecked()`로 충돌 검사에서 제외
- 충돌 가능 구간을 `distance <= 2.0f`로 제한
- 가로 충돌은 플레이어와 장애물의 정규화된 위치 및 반폭으로 판정
- 저해상도 화면에서 가장자리 접촉이 억울하지 않도록 충돌 범위를
  보이는 스프라이트보다 조금 작게 설정

```cpp
if (obstacle->GetDistance() < -2.0f)
{
    obstacle->MarkChecked();
    continue;
}

const bool horizontalOverlap =
    std::abs(playerX - obstacleX) < playerHalfWidth + obstacleHalfWidth;

const bool reachesPlayer =
    obstacle->GetDistance() <= 2.0f
    && playerScreenY == obstacleScreenY;
```

### 현재 충돌 규칙

- 노란 가시: 좌우로 피하거나 충분히 높게 점프하면 통과
- 청록색 얼음벽: 좌우로 피해야 함
- 장애물 가장자리를 살짝 스치는 경우는 충돌하지 않음
- 이미 통과한 장애물은 다시 충돌하지 않음

### 검증

Visual Studio MSBuild로 `Debug|x64` 빌드를 확인했다.

- 경고: 0
- 오류: 0
