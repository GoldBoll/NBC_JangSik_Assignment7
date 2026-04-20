#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "NBC_AssignmentPawn.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

UCLASS()
class NBC_ASSIGNMENT_API ANBC_AssignmentPawn : public APawn
{
	GENERATED_BODY()

public:
	ANBC_AssignmentPawn();

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaTime) override;

	// ── 컴포넌트 ──────────────────────────────────────────
	// 충돌 판정 루트 (박스 반크기 XY=50, Z=15 — 드론 형태에 맞게 납작하게 설정)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> BoxComp;

	// 드론 비주얼 메시 (SM_Drone 자동 로드)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	// 카메라를 일정 거리(400) 뒤에 붙잡아두는 암
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> SpringArmComp;

	// 플레이어 시점 카메라
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> CameraComp;

	// ── Enhanced Input ────────────────────────────────────
	// 에디터에서 IMC_Drone 에셋을 여기에 할당
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> InputMappingContext;

	// W/A/S/D 수평 이동 (Axis2D)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	// Space(+1) / Shift(-1) 수직 이동 (Axis1D)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveUpAction;

	// 마우스 휠 Roll 회전 (Axis1D)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> RollAction;

	// 마우스 XY Yaw/Pitch 회전 (Axis2D)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	// ── 이동 파라미터 ──────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MoveSpeed = 600.f;         // 수평/수직 이동 속도 (cm/s)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float LookSensitivity = 1.f;     // 마우스/휠 회전 감도 배율

	// ── 물리 파라미터 ──────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float GravityZ = -980.f;         // 중력 가속도 (cm/s²)

	// ── 물리 상태 ─────────────────────────────────────────
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Physics")
	float FallVelocity = 0.f;        // 현재 낙하 속도 — 매 프레임 GravityZ 누적

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Physics")
	bool bIsGrounded = false;        // 이번 프레임에 바닥 LineTrace가 맞았는지

	// Space 입력 프레임에 true → Tick에서 중력 누적 억제, 프레임 끝에 리셋
	bool bAscending = false;

	// 발밑 LineTrace로 착지 여부 반환
	bool CheckGround(FHitResult& OutHit);

	// ── 입력 핸들러 ───────────────────────────────────────
	void MoveUp(const FInputActionValue& Value);  // 수직 이동 + bAscending 세팅
	void Move(const FInputActionValue& Value);    // Yaw 기준 수평 이동
	void Roll(const FInputActionValue& Value);    // Z축 롤 회전
	void Look(const FInputActionValue& Value);    // Yaw/Pitch 회전
};
