# PolarRunner 트러블슈팅 — 2026-08-14

## Enemy 제거 직후 해안 화면이 사라지는 문제

### 증상

- 해안 구간에서 Enemy를 격추하면 배와 바다 배경이 같은 순간에 사라짐
- 다음 지형이 즉시 나타나 화면이 교체되는 것처럼 보임
- Enemy 전투가 끝난 뒤 해안에서 빠져나가는 여운이 부족함

### 원인

해안 유지 여부를 현재 Enemy가 살아 있는지만으로 판정하고 있었다.

```cpp
const bool keepCoastVisible =
    coastEnemy && !coastEnemy->HasExpired();
```

따라서 Enemy가 `Destroy()`된 프레임부터 `keepCoastVisible`이 바로
`false`가 되고, 렌더링 지형도 즉시 원래 코스 지형으로 복귀했다.

### 수정

Enemy가 제거된 순간 `coastVisualHoldTimer`를 2.5초로 설정했다. 잔류
시간 동안에는 Enemy가 등장했던 바다 방향도 `heldCoastOceanOnRight`에
저장해 동일한 해안 화면을 유지한다.

```cpp
if (coastEnemyWasActive && !hasActiveEnemy)
{
    coastVisualHoldTimer = 2.5f;
}

const bool keepCoastVisible =
    (coastEnemy && !coastEnemy->HasExpired())
    || coastVisualHoldTimer > 0.0f;
```

타이머가 끝난 뒤에만 현재 코스의 원래 지형으로 복귀한다. 실제 도로
좌표, 장애물, 충돌 판정은 바꾸지 않고 렌더링 상태만 유지하도록 했다.

### 검증

- `Debug|x64` 컴파일 성공
- 경고 0개, 오류 0개
- Enemy 격추 후 해안이 약 2.5초 유지되는지 플레이 확인 필요
- 해안 유지 중 바다 방향이 바뀌지 않는지 플레이 확인 필요
- 원래 지형으로 복귀할 때 장애물과 충돌 판정이 정상인지 확인 필요
