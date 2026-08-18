# PolarRunner

남극을 배경으로 한 C++ 콘솔 러너·슈팅 게임입니다.
펭귄을 조작해 5000m 코스를 달리며 장애물을 피하고, 별을 모아 얻은 SHOT으로
해안에 나타나는 밀렵선과 전투합니다.

원티드 포텐업 게임 개발자 양성과정 5기 · 개인 프로젝트

```
             _~_
            (o o)
            / V \
           /( _ )\
             ^ ^
```

## 게임 소개

거리를 진행할수록 지형과 장애물, 적의 난이도가 단계적으로 변합니다.

| 단계 | 내용 |
|---|---|
| 달리기 | 좌우로 이동하며 코스를 주행. 지형에 따라 마찰이 달라짐 |
| 장애물 회피 | 가시는 점프로, 얼음벽은 좌우로 회피 |
| 별 수집 | 같은 레인의 별을 수집. 5개마다 SHOT 1발 획득 |
| 사격 | `F`로 그물을 발사 |
| 해안 전투 | 바다 쪽에서 나타난 밀렵선이 접근·추격하며 사격 |
| 완주 | 5000m 도달 시 GOAL |

## 조작

### 타이틀 화면

| 키 | 동작 |
|---|---|
| `Enter` | 게임 시작 |
| `B` | 다리 구간부터 시작 (테스트) |
| `T` | 사격 테스트 레벨 |
| `Esc` | 종료 |

### 플레이 중

| 키 | 동작 |
|---|---|
| `←` `→` | 좌우 이동 |
| `↑` `↓` | 앞뒤 위치 조정 |
| `Space` | 점프 |
| `F` | SHOT 발사 (SHOT 보유 시, 점프 중에는 불가) |
| `Esc` | 일시정지 메뉴 |

방향키를 놓아도 관성으로 미끄러집니다. 특히 좁은 얼음길에서는 훨씬 크게
밀리므로 미리 반대로 꺾어 보정해야 합니다.

## 지형과 장애물

### 지형

| 지형 | 특징 |
|---|---|
| `Snowfield` | 기본 설원. 넓은 도로 |
| `Coast` | 해안. 밀렵선이 등장하는 전투 구간 |
| `Canyon` | 협곡. 도로 폭이 크게 좁아짐 |
| `NarrowIcePath` | 좁은 얼음길. 마찰이 낮아 미끄러짐이 심함 |
| `BrokenIce` | 갈라진 얼음 지대 |
| `ResearchBase` | 연구기지 |

### 장애물

| 장애물 | 회피 방법 |
|---|---|
| `LowSpike` `^^^^` | 좌우 회피 또는 점프 (논리 높이 0.4 이상) |
| `IceWall` `\|####\|` | 좌우 회피만 가능. 점프로 넘을 수 없음 |
| `BrokenBridge` | 점프 (논리 높이 0.6 이상). 실패 시 추락 |
| `Puddle` `~~~~` | 물웅덩이 |

랜덤 배치되는 구간에서도 항상 통과 가능한 레인이 하나 이상 남도록
생성 단계에서 검증합니다.

## 실행 방법

### 요구 사항

- Visual Studio (C++ 데스크톱 개발 워크로드)
- Windows

### 빌드

```
PolarRunner.slnx 를 Visual Studio에서 열고 Debug | x64 로 빌드
```

명령줄에서 빌드하려면 다음과 같이 실행합니다.

```
MSBuild.exe PolarRunner.slnx /t:Build /p:Configuration=Debug /p:Platform=x64
```

빌드 결과물은 `Bin/x64/Debug/PolarRunner.exe` 에 생성됩니다.

### 화면 설정

`Config/Setting.txt` 에서 콘솔 크기와 프레임레이트를 조정할 수 있습니다.

```
framerate = 120
width = 80
height = 30
```

## 프로젝트 구조

교육 과정에서 학습한 `Engine` / `Level` / `Actor` 구조를 그대로 적용했습니다.

```
CraftEngine/          엔진 (DLL)
├─ Engine/            메인 루프, 레벨 전환, 보조 레벨 오버레이
├─ Level/             액터 목록 관리, 생성·소멸 처리
├─ Actor/             액터 기반 클래스
├─ Physics/           swept AABB 충돌 검사
├─ Render/            렌더러, 스크린 버퍼
├─ Input/             키 입력
└─ Math/              Vector2, Color

PolarRunner/          게임 (EXE)
├─ Level/
│  ├─ TitleLevel      타이틀 화면
│  ├─ PolarLevel      본 게임 (코스, 렌더링, 충돌 판정)
│  ├─ MenuLevel       일시정지 메뉴 (오버레이)
│  └─ TestLevel       사격 테스트
├─ Actor/
│  ├─ Player          펭귄
│  ├─ Enemy           밀렵선
│  ├─ PolarObstacle   장애물
│  ├─ PolarStar       별
│  ├─ PlayerBullet    플레이어 그물
│  └─ EnemyBullet     적 탄환
└─ Game/
   └─ ObstacleType    장애물 종류
```

## 주요 구현

### 의사 3D 원근 투영

진행 거리를 화면 Y 좌표에 비선형으로 매핑해 원근감을 만듭니다.
화면 Y를 한 행씩 훑으면서 그 행의 깊이·도로 중심·폭·지형을 계산해 그립니다.

```cpp
const float normalized = 1.0f - distance / viewDistance;
const float perspective = std::pow(normalized, 1.55f);
```

커브는 깊이에 smoothstep을 곱해 적용합니다. 먼 행은 거의 움직이지 않고
가까울수록 크게 휘어, 지평선에서 도로가 접혀 보이는 문제를 막았습니다.

### 표현과 판정의 분리

엔진의 충돌은 "높이 1줄, 폭 = 이미지 문자열 길이"인 사각형 하나만 지원합니다.
여러 줄 ASCII나 공백이 섞인 그림은 그대로 넣을 수 없어, 이미지 문자열을
**히트박스 선언으로만** 쓰고 실제 그림은 `Draw()`에서 행마다 나눠 출력합니다.

같은 원칙으로 점프도 게임 규칙용 논리 높이와 화면 출력용 위치를 분리했습니다.

```cpp
float GetJumpHeight() const;       // 게임 판정용 0.0 ~ 1.0
int   GetJumpScreenOffset() const; // 화면 출력 위치
```

ASCII 모양을 바꿔도 충돌 규칙과 난이도가 변하지 않습니다.

### Enemy 상태 전환

밀렵선은 등장하자마자 공격하지 않고 접근 과정을 거칩니다.

```
ApproachingDepth  →  ClosingSide  →  Chasing
   거리 좁히기        사격 위치로 이동      추격하며 사격
```

바다가 그려진 방향에서만 스폰되며, 좌우 어느 쪽에서 나타나도 선체 ASCII와
탄환 방향이 대칭으로 동작합니다.

### 일시정지 메뉴 오버레이

기존에는 `PolarLevel` 내부에서 메뉴 상태를 함께 관리했습니다. 이를 별도
`MenuLevel`로 분리하고, 엔진에 보조 레벨 기능을 추가했습니다.

| | Tick | Draw |
|---|---|---|
| `PolarLevel` (주 레벨) | 정지 | 유지 |
| `MenuLevel` (보조 레벨) | 실행 | 실행 |

진행 거리, 장애물, Enemy 상태는 그대로 유지한 채 메뉴만 화면 위에
겹쳐 표시됩니다.

## 개발 기록

개발 과정에서 겪은 문제와 해결 과정을 `Docs/` 에 정리했습니다.

| 문서 | 내용 |
|---|---|
| [260812](Docs/260812_Troubleshooting.md) | 점프 판정, LowSpike·BrokenBridge 충돌 시점 |
| [260813](Docs/260813_Troubleshooting.md) | 스폰 직후 탄환 오판정, Enemy 좌우 대칭 스폰 |
| [260814 IceWall](Docs/260814_IceWallCollision_Troubleshooting.md) | IceWall 충돌 판정 |
| [260814 Enemy 지형](Docs/260814_EnemyCoastTransition_Troubleshooting.md) | Enemy 제거 후 해안 유지 |
| [260814 별·HUD](Docs/260814_StarCollectionAndHud_Troubleshooting.md) | 별 수집 판정과 HUD |
| [260818 정리](Docs/260818_Presentation_Troubleshooting.md) | IceWall·원근감·슈팅 종합 정리 |

대표적인 사례 두 가지입니다.

**커브 구간 IceWall 오충돌** — 화면에는 닿지 않았는데 충돌이 발생했습니다.
원인은 렌더링과 충돌 판정이 IceWall의 X 좌표를 서로 다른 화면 행에서
계산한 것이었습니다. 커브에서는 행마다 도로 중심이 달라지기 때문입니다.
판정도 렌더링과 같은 행·같은 좌표를 쓰도록 통일해 해결했습니다.

**스폰 직후 탄환 오판정** — 적이 접근하지도 않았는데 피격 판정이 났습니다.
`Actor` 생성자가 `previousPosition`을 초기화하지 않아, 스폰 첫 프레임에
`(0, 0)`에서 스폰 좌표까지 이어지는 거대한 swept 선분이 화면을 가로지른
것이 원인이었습니다. 생성자 초기화 리스트에 `previousPosition(position)`을
추가해 해결했습니다.
