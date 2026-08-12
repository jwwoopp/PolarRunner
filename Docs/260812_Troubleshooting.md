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
float PolarPlayer::GetJumpHeight() const
{
    if (!isJumping)
    {
        return 0.0f;
    }

    const float progress = jumpTimer / jumpDuration;
    return std::sin(progress * 3.14159265f);
}

int PolarPlayer::GetJumpScreenOffset() const
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
