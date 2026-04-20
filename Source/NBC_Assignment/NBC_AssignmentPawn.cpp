#include "NBC_AssignmentPawn.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

// 컴포넌트 생성 및 기본값 설정 — 에디터 실행 전에 한 번만 호출됨
ANBC_AssignmentPawn::ANBC_AssignmentPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// 루트: 드론 형태에 맞게 납작한 박스 (반크기 XY=50, Z=15)
	BoxComp = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComp"));
	BoxComp->SetBoxExtent(FVector(50.f, 50.f, 15.f));
	SetRootComponent(BoxComp);

	// 드론 메시 — 에셋 경로에서 SM_Drone 자동 로드
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(BoxComp);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DroneMesh(TEXT("/Game/NBC/Drone/SM_Drone"));
	if (DroneMesh.Succeeded())
		MeshComp->SetStaticMesh(DroneMesh.Object);

	// 스프링암 — 카메라를 드론 뒤 400cm에 고정, Pawn 회전과 별개로 움직임
	SpringArmComp = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComp"));
	SpringArmComp->SetupAttachment(BoxComp);
	SpringArmComp->TargetArmLength = 400.f;
	SpringArmComp->bUsePawnControlRotation = false;

	// 카메라 — 스프링암 끝 소켓에 부착
	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
	CameraComp->SetupAttachment(SpringArmComp, USpringArmComponent::SocketName);
	CameraComp->bUsePawnControlRotation = false;

	// 별도 PlayerController 없이 Player0 가 바로 빙의
	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

// 게임 시작 시 IMC_Drone 을 Enhanced Input 서브시스템에 등록
void ANBC_AssignmentPawn::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (InputMappingContext)
				Sub->AddMappingContext(InputMappingContext, 0);
		}
	}
}

// IA 액션과 핸들러 함수를 연결 — ETriggerEvent::Triggered = 키를 누르는 매 프레임 호출
void ANBC_AssignmentPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EIC->BindAction(MoveAction,   ETriggerEvent::Triggered, this, &ANBC_AssignmentPawn::Move);
		EIC->BindAction(MoveUpAction, ETriggerEvent::Triggered, this, &ANBC_AssignmentPawn::MoveUp);
		EIC->BindAction(RollAction,   ETriggerEvent::Triggered, this, &ANBC_AssignmentPawn::Roll);
		EIC->BindAction(LookAction,   ETriggerEvent::Triggered, this, &ANBC_AssignmentPawn::Look);
	}
}

// 매 프레임 물리 처리: 착지 스냅 / 중력 / 바닥 뚫기 방지
void ANBC_AssignmentPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FHitResult HitResult;
	bIsGrounded = CheckGround(HitResult);

	const FVector Location = GetActorLocation();

	// 박스 로컬 바닥 중심을 월드 변환 — 기울어진 상태에서도 실제 바닥 접촉점 기준
	const FVector WorldBottom = GetActorTransform().TransformPosition(FVector(0.f, 0.f, -BoxComp->GetScaledBoxExtent().Z));
	// 액터 중심에서 박스 월드 바닥까지의 Z 거리 (기울기에 따라 변함)
	const float BottomOffset = Location.Z - WorldBottom.Z;

	if (bIsGrounded)
	{
		// 착지 상태: 낙하 속도 초기화 + 바닥 위로 스냅
		FallVelocity = 0.f;
		const float GroundZ = HitResult.ImpactPoint.Z + BottomOffset;
		if (Location.Z <= GroundZ + 1.f)
		{
			FVector SnapLoc = Location;
			SnapLoc.Z = GroundZ;
			SetActorLocation(SnapLoc);
		}
	}
	else
	{
		if (bAscending)
		{
			// Space 입력 프레임: 중력 누적 억제 (드론이 상승할 수 있도록)
			FallVelocity = 0.f;
		}
		else
		{
			// 공중: 중력 누적 후 다음 프레임 박스 바닥 위치를 예측해 바닥 뚫기 방지
			FallVelocity += GravityZ * DeltaTime;
			const float FallOffset = FallVelocity * DeltaTime;

			// 예측 위치: 액터가 FallOffset 만큼 내려갔을 때의 박스 바닥
			const FVector PredictedBottom = WorldBottom + FVector(0, 0, FallOffset);
			FHitResult LandHit;
			FCollisionQueryParams LandParams;
			LandParams.AddIgnoredActor(this);

			// 예측 바닥에서 1cm LineTrace — 충돌 시 이동 전에 스냅해서 바닥을 뚫지 않음
			if (GetWorld()->LineTraceSingleByChannel(LandHit, PredictedBottom, PredictedBottom + FVector(0, 0, -1.f), ECC_Visibility, LandParams))
			{
				FVector SnapLoc = Location;
				SnapLoc.Z = LandHit.ImpactPoint.Z + BottomOffset;
				SetActorLocation(SnapLoc);
				FallVelocity = 0.f;
				bIsGrounded = true;
			}
			else
			{
				AddActorWorldOffset(FVector(0, 0, FallOffset), false);
			}
		}

		// bAscending은 MoveUp()에서 세팅, 매 프레임 끝에 리셋
		bAscending = false;
	}
}

// 박스 로컬 바닥을 월드 변환한 위치에서 1cm 아래로 LineTrace — 기울기와 무관하게 정확한 착지 감지
bool ANBC_AssignmentPawn::CheckGround(FHitResult& OutHit)
{
	const FVector WorldBottom = GetActorTransform().TransformPosition(FVector(0.f, 0.f, -BoxComp->GetScaledBoxExtent().Z));
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	return GetWorld()->LineTraceSingleByChannel(
		OutHit,
		WorldBottom,
		WorldBottom + FVector(0, 0, -1.f),
		ECC_Visibility,
		Params);
}

void ANBC_AssignmentPawn::MoveUp(const FInputActionValue& Value)
{
	const float V = Value.Get<float>();
	// 착지 상태에서 하강 입력(Shift) 무시
	if (bIsGrounded && V < 0.f) return;
	if (V > 0.f) bAscending = true;
	// GetActorUpVector()는 월드 공간 벡터이므로 AddActorWorldOffset으로 적용
	AddActorWorldOffset(GetActorUpVector() * V * MoveSpeed * GetWorld()->GetDeltaSeconds(), true);
}

void ANBC_AssignmentPawn::Move(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	const float DeltaTime = GetWorld()->GetDeltaSeconds();

	// Yaw 기준 수평 이동 — Pitch 기울기 무관하게 카메라 방향으로 이동
	const FRotator YawOnly = FRotator(0.f, GetActorRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawOnly).GetScaledAxis(EAxis::X);
	const FVector Right   = FRotationMatrix(YawOnly).GetScaledAxis(EAxis::Y);

	AddActorWorldOffset((Forward * V.X + Right * V.Y) * MoveSpeed * DeltaTime, true);
}

void ANBC_AssignmentPawn::Roll(const FInputActionValue& Value)
{
	AddActorLocalRotation(FRotator(0.f, 0.f, Value.Get<float>() * LookSensitivity));
}

void ANBC_AssignmentPawn::Look(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	// 착지 상태에서 Pitch(마우스 상하) 무시, Yaw(좌우)는 허용
	const float Pitch = bIsGrounded ? 0.f : V.Y;
	AddActorLocalRotation(FRotator(Pitch * LookSensitivity, V.X * LookSensitivity, 0.f));
}
