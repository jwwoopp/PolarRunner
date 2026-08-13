# PolarRunner 트러블슈팅 — 2026-08-13

## 스폰 직후 오판정되는 Enemy 탄환 CRASH

### 증상

- 해안 구간에서 화면 오른쪽에 스폰된 Enemy가 실제로는 플레이어 근처에
  가지도 않았는데 `CRASH: HIT BY ENEMY BULLET`이 표시됨
- 왼쪽에서 스폰될 때는 상대적으로 덜 발생하고, 오른쪽 스폰에서 두드러짐

### 원인

`Craft::Actor` 생성자가 `previousPosition`을 초기화하지 않아 기본값
`(0, 0)`으로 남아 있었다. `CollisionSystem::Test()`와
`CheckEnemyBulletCollisions()`는 탄환의 이전 위치와 현재 위치를 잇는
스윕(swept) 선분으로 충돌을 판정하는데, 스폰 직후 첫 프레임에는
`previousPosition`이 `(0, 0)`(화면 왼쪽 위)이고 `position`은 실제 스폰
좌표이므로 그 사이의 거대한 스윕 선분이 화면을 가로질렀다. 화면 오른쪽에서
스폰된 탄환일수록 이 선분이 플레이어 위치를 지나갈 확률이 높아 오판정이
두드러졌다.

### 수정

`Actor` 생성자 초기화 리스트에 `previousPosition(position)`을 추가해
스폰 직후 첫 프레임에는 이전 위치와 현재 위치가 같도록 했다.

```cpp
Actor::Actor(
    const std::string& image,
    const Vector2& position,
    Color color
)
    : image(image), position(position), previousPosition(position),
    color(color), width(static_cast<int>(image.size()))
{
}
```

같은 `previousPosition`을 `CollisionSystem::Test()`도 사용하므로, 이
버그는 PlayerBullet-Enemy 충돌 등 스폰 직후 발생하는 다른 충돌 판정에도
잠재적으로 영향이 있었을 것으로 보인다.

### 검증

- Visual Studio MSBuild로 `Debug|x64` 빌드 성공 (경고 0, 오류 0)
- 스테이징된 변경분만 별도 워크트리에서 독립적으로도 빌드되는지 확인
- 기존 `PlaytestLog.log` 플레이 기록에서 Enemy 발사와 CRASH 로그 사이에
  항상 실제 탄환 이동 시간만큼의 간격이 있고, 스폰/발사와 동일 프레임에
  즉시 CRASH가 발생하는 사례(버그 당시 패턴)는 없음을 확인

---

## 해안 Enemy 좌우 대칭 스폰

### 문제 현상

기존에는 해안 Enemy가 항상 화면 왼쪽에서만 스폰됐다. 접근 위치, 정차
위치, 탄환 발사 방향이 모두 왼쪽 스폰을 전제로 하드코딩되어 있었다.

### 수정

`UpdateCoastEnemy()`에서 `EnemySide::Left`/`Right`를 50/50 랜덤으로
선택하고, 접근·정차 위치와 `EnemyBullet` 발사 방향(음수 속도)까지 좌우
대칭으로 일반화했다.

```cpp
static std::mt19937 coastEnemySideRandom(std::random_device{}());
const EnemySide spawnSide =
    std::uniform_int_distribution<int>(0, 1)(coastEnemySideRandom) == 0
        ? EnemySide::Left : EnemySide::Right;
coastEnemy = SpawnActor<Enemy>(initialDistance, spawnSide);
coastEnemyScreenX = spawnSide == EnemySide::Left
    ? screenWidth * 0.18f : screenWidth * 0.82f;
```

`Enemy::Draw()`도 `side == Right`일 때 선체 ASCII 아트를 좌우 반전해
포문이 도로 쪽을 향하도록 맞췄고, `EnemyBullet`은 왼쪽으로 발사될 때도
화면 왼쪽 경계(`x <= 0`)에서 `Destroy()`되도록 수정했다.

### 검증

- Visual Studio MSBuild로 `Debug|x64` 빌드 성공 (경고 0, 오류 0)
- `PlaytestLog.log`에서 `side=Left`, `side=Right` 스폰이 모두 발생하고,
  양쪽 모두 Chasing 진입·사격·격추가 정상적으로 이어짐을 확인
