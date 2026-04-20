# NBC_JangSik_Assignment7 — C++ 6DOF 드론 Pawn

<video src="https://github.com/GoldBoll/NBC_JangSik_Assignment7/raw/master/docs/demo.mp4" controls width="100%"></video>

언리얼 엔진 C++ 로 구현한 6자유도(6DOF) 드론 Pawn.  
물리 중력, 착지 스냅, Enhanced Input 시스템을 직접 구현.

---

## 조작 키

| 키 | 동작 |
|---|---|
| W / S | 전진 / 후진 |
| A / D | 좌 / 우 |
| Space | 상승 |
| Left Shift | 하강 |
| 마우스 XY | Yaw / Pitch 회전 |
| 마우스 휠 | Roll 회전 |

---

## 컴포넌트 구조

```
BoxComp (루트 콜리전 — 반크기 XY=50, Z=15)
├─ MeshComp (SM_Drone)
├─ SpringArmComp (400cm)
│  └─ CameraComp
```

- **BoxComponent**: 드론 형태에 맞게 XYZ 개별 조정 가능한 박스 콜리전 사용
- **SpringArm**: `bUsePawnControlRotation = false` — Pawn 회전과 독립적으로 카메라 위치 유지
- **AutoPossessPlayer**: PlayerController 없이 Player0 가 바로 빙의

---

## Enhanced Input 세팅

### IA Value Type

| IA | Value Type |
|---|---|
| IA_Move | Axis2D (Vector2D) |
| IA_Look | Axis2D (Vector2D) |
| IA_MoveUp | Axis1D (float) |
| IA_Roll | Axis1D (float) |

### IMC_Drone 키 매핑

| IA | 키 | Modifier |
|---|---|---|
| IA_Move | W | 없음 |
| | S | Negate |
| | D | Swizzle (YXZ) |
| | A | Swizzle (YXZ) + Negate |
| IA_Look | Mouse XY 2D-Axis | 없음 |
| IA_MoveUp | Space | 없음 (+1) |
| | Left Shift | Negate (-1) |
| IA_Roll | Mouse Wheel Axis | 없음 |

> Triggers / Modifiers는 IA 에셋이 아닌 IMC에서 키별로 설정한다.

---

## 물리 구현

### 중력 시스템

커스텀 중력을 Tick에서 직접 구현 (`GravityZ = -980 cm/s²`).  
`FallVelocity`를 매 프레임 누적하고 `CheckGround`로 착지 여부를 판단한다.

```
매 프레임 Tick:
  WorldBottom = TransformPosition(로컬 바닥점)  ← 기울기 반영
  CheckGround(WorldBottom) → bIsGrounded

  if 착지:
    FallVelocity = 0
    액터 Z = ImpactPoint.Z + BottomOffset  ← 스냅

  else if Space 입력(bAscending):
    FallVelocity = 0  ← 중력 억제

  else:
    FallVelocity += GravityZ * DeltaTime
    예측 바닥 위치에서 LineTrace
      충돌 → 이동 전 스냅  ← 바닥 뚫기 방지
      없으면 → AddActorWorldOffset
```

### 바닥 감지 (CheckGround)

박스 로컬 바닥점 `(0, 0, -HH)` 을 월드 변환 후 트레이스.  
액터 중심에서 쏘면 기울어진 상태에서 실제 바닥 접촉점을 놓친다.

```cpp
const FVector WorldBottom = GetActorTransform().TransformPosition(
    FVector(0.f, 0.f, -BoxComp->GetScaledBoxExtent().Z));
GetWorld()->LineTraceSingleByChannel(
    OutHit, WorldBottom, WorldBottom + FVector(0, 0, -1.f), ECC_Visibility, Params);
```

### 수평 이동 (WASD)

`GetActorForwardVector()` 는 Pitch 기울기를 포함해 방향이 틀어지므로  
Yaw 만 추출한 회전 행렬로 수평 Forward/Right 를 계산한다.

```cpp
const FRotator YawOnly = FRotator(0.f, GetActorRotation().Yaw, 0.f);
const FVector Forward = FRotationMatrix(YawOnly).GetScaledAxis(EAxis::X);
const FVector Right   = FRotationMatrix(YawOnly).GetScaledAxis(EAxis::Y);
AddActorWorldOffset((Forward * V.X + Right * V.Y) * MoveSpeed * DeltaTime, true);
```

### 수직 이동 (Space / Shift)

`GetActorUpVector()` 는 월드 공간 벡터이므로 `AddActorWorldOffset` 으로 적용.  
`AddActorLocalOffset` 에 넘기면 이중 회전이 적용되어 방향이 틀어진다.

```cpp
AddActorWorldOffset(GetActorUpVector() * V * MoveSpeed * DeltaTime, true);
```

착지 중 Shift(하강) 와 마우스 Pitch 는 차단된다.

---

## 파라미터 (에디터에서 조정 가능)

| 변수 | 기본값 | 설명 |
|---|---|---|
| `MoveSpeed` | 600 cm/s | 수평 / 수직 이동 속도 |
| `LookSensitivity` | 1.0 | 마우스 / 휠 감도 |
| `GravityZ` | -980 cm/s² | 중력 가속도 |

---

## 트러블슈팅 요약

| 증상 | 원인 | 해결 |
|---|---|---|
| Space 누르면 통통 튐 | FallVelocity 누적으로 바닥 터널링 후 스냅 반동 | 예측 위치에서 LineTrace → 이동 전 스냅 |
| Space 눌러도 상승 안 됨 | GravityZ(-980) > MoveSpeed(600) | `bAscending` 플래그로 Space 누르는 동안 중력 억제 |
| WASD 방향 이상 (기울인 후) | `GetActorForwardVector()` 가 Pitch 포함 | Yaw 전용 FRotationMatrix 사용 |
| Space 가 월드 Z 로만 이동 | `AddActorLocalOffset(GetActorUpVector())` 이중 회전 | `AddActorWorldOffset(GetActorUpVector())` 로 변경 |
| 기울어진 채 착지 시 바닥 뚫음 | LineTrace 시작점이 액터 중심 | 로컬 바닥점을 월드 변환 후 트레이스 |
